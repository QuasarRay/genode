/*
 * \brief  Genode USB client provider C-API
 * \author Sebastian Sumpf
 * \author Stefan Kalkowski
 * \date   2023-06-29
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/heap.h>
#include <base/tslab.h>
#include <util/reconstructible.h>
#include <util/list_model.h>
#include <util/string.h>

#include <usb_session/device.h>
#include <genode_c_api/usb_client.h>

using namespace Genode;

/*
 * We use a stripped down version of the Id_space here without a mutex,
 * because we need to re-enter the space when different cooperatively
 * scheduled user-tasks block inside the insertion of new device-requests
 * and the handling of URBs.
 */
template <typename T>
class Id_space : public Noncopyable
{
	public:

		struct Id
		{
			unsigned long value;

			bool operator == (Id const &other) const { return value == other.value; }

			void print(Output &out) const { Genode::print(out, value); }
		};

		class Element : public Avl_node<Element>
		{
			private:

				T        &_obj;
				Id_space &_space;
				Id        _id { 0 };

				friend class Id_space;

				Element *_lookup(Id id)
				{
					if (id.value == _id.value) return this;

					Element *e = Avl_node<Element>::child(id.value > _id.value);

					return e ? e->_lookup(id) : 0;
				}

				template <typename ARG>
				void _for_each(auto const &fn) const
				{
					if (Avl_node<Element>::child(Avl_node_base::LEFT))
						Avl_node<Element>::child(Avl_node_base::LEFT)->template _for_each<ARG>(fn);

					fn(static_cast<ARG &>(_obj));

					if (Avl_node<Element>::child(Avl_node_base::RIGHT))
						Avl_node<Element>::child(Avl_node_base::RIGHT)->template _for_each<ARG>(fn);
				}

			public:

				Element(T &obj, Id_space &space)
				:
					_obj(obj), _space(space)
				{
					_id = space._unused_id();
					_space._elements.insert(this);
				}

				~Element()
				{
					_space._elements.remove(this);
				}

				bool higher(Element *other) { return other->_id.value > _id.value; }

				Id id() const { return _id; }
		};

	private:
 
		Avl_tree<Element> _elements { };
		unsigned long     _cnt = 0;

		Id _unused_id()
		{
			unsigned long _attempts = 0;
			for (; _attempts < ~0UL; _attempts++, _cnt++) {

				Id const id { _cnt };

				/* another attempt if is already in use */
				if (_elements.first() && _elements.first()->_lookup(id))
					continue;

				return id;
			}
			/*
			 * The number of IDs exhausts the number of unsigned long values.
			 * In this hypothetical case, accept ID ambiguities.
			 */
			return { ~0UL };
		}

	public:

		template <typename ARG>
		void for_each(auto const &fn) const
		{
			if (_elements.first())
				_elements.first()->template _for_each<ARG>(fn);
		}

		template <typename ARG, typename FN>
		auto apply(Id id, FN const &fn, auto const &missing_fn)
		-> typename Trait::Functor<decltype(&FN::operator())>::Return_type
		{
			if (_elements.first())
				if (Element *e = _elements.first()->_lookup(id))
					return fn(static_cast<ARG &>(e->_obj));

			return missing_fn();
		}

		~Id_space()
		{
			if (_elements.first())
				error("ID space not empty at destruction time");
		}
};


struct Device;

struct Endpoint : List_model<Endpoint>::Element
{
	uint8_t  const address;
	uint8_t  const attributes;
	uint16_t const max_packet_size;

	Endpoint(Node const &n)
	:
		address(n.attribute_value<uint8_t>("address", 0xff)),
		attributes(n.attribute_value<uint8_t>("attributes", 0xff)),
		max_packet_size(n.attribute_value<uint16_t>("max_packet_size", 0)) {}

	bool matches(Node const &node) const {
		return address == node.attribute_value<uint8_t>("address", 0xff); }

	static bool type_matches(Node const &node) {
		return node.has_type("endpoint"); }
};


class Interface : ::Id_space<::Interface>::Element
{
	public:

		class Urb : Usb::Endpoint, public Usb::Interface::Urb
		{
			private:

				friend class ::Interface;
				using Pdesc = Usb::Interface::Packet_descriptor;

				static Pdesc::Type _type(genode_usb_client_iface_type_t t)
				{
					switch(t) {
						case ::BULK:  return Pdesc::BULK;
						case ::IRQ:   return Pdesc::IRQ;
						case ::ISOC:  return Pdesc::ISOC;
						case ::FLUSH: return Pdesc::FLUSH;
					};
					return Pdesc::FLUSH;
				}

				struct Driver_data { void * const data; } _driver_data;

			public:

				Urb(Usb::Interface                &iface,
				    ::Endpoint                    &endp,
				    genode_usb_client_iface_type_t type,
				    size_t                         size,
				    void                          *opaque_data)
				:
					Usb::Endpoint(endp.address, endp.attributes),
					Usb::Interface::Urb(iface, *this, _type(type),
					                    size, 0),
					_driver_data{opaque_data} {}
		};

		struct Element : List_model<Element>::Element
		{
			::Interface &interface;

			Element(::Interface &iface) : interface(iface) {}

			bool matches(Node const &n) const
			{
				uint8_t nr  = n.attribute_value<uint8_t>("number", 0xff);
				uint8_t alt = n.attribute_value<uint8_t>("alt_setting", 0xff);
				return interface._number == nr && interface._alt_setting == alt;
			}

			static bool type_matches(Node const &node) {
				return node.has_type("interface"); }
		};

	private:

		friend class Device;

		Device                       &_device;
		Constructible<Usb::Interface> _iface {};
		List_model<Endpoint>          _endpoints {};
		uint8_t const                 _number;
		uint8_t const                 _alt_setting;
		bool                          _active;
		Tslab<Urb, 4096>              _slab;

		/* The interface's buffer size might be configureable in the future */
		size_t _buf_size { 4096 * 128 };

		Reconstructible<Element> _list_model_elem { *this };

	public:

		Interface(Device &device, Node const &n, Allocator &alloc);

		~Interface()
		{
			if (_iface.constructed())
				_iface->dissolve_all_urbs<Urb>([&] (Urb &urb) {
					destroy(_slab, &urb); });
		}

		void with_session(auto const &fn);

		uint8_t number()      const { return _number; };
		uint8_t alt_setting() const { return _alt_setting; };
		bool    active()      const { return _active; }

		void active(bool active) { _active = active; }

		Allocator &slab() { return _slab; }

		bool matches(Node const &n) const
		{
			uint8_t nr  = n.attribute_value<uint8_t>("number", 0xff);
			uint8_t alt = n.attribute_value<uint8_t>("alt_setting", 0xff);
			return _number == nr && _alt_setting == alt;
		}

		static bool type_matches(Node const &node) {
			return node.has_type("interface"); }

		void update(Allocator &alloc, Node const &node)
		{
			_active = node.attribute_value("active", false);
			_endpoints.update_from_node(node,

				/* create */
				[&] (Node const &node) -> Endpoint & {
					return *new (alloc) Endpoint(node); },

				/* destroy */
				[&] (Endpoint &endp) {
					destroy(alloc, &endp); },

				/* update */
				[&] (Endpoint &, Node const &) { }
			);
		}

		void update(genode_usb_client_produce_out_t      out,
		            genode_usb_client_consume_in_t       in,
		            genode_usb_client_produce_out_isoc_t out_isoc,
		            genode_usb_client_consume_in_isoc_t  in_isoc,
		            auto const &complete)
		{
			if (!_iface.constructed())
				return;

			_iface->update_urbs<Urb>(

				/* produce out content */
				[&] (Urb &urb, Byte_range_ptr &dst) {
					out(urb._driver_data.data, { dst.start, dst.num_bytes });
				},

				/* consume in results */
				[&] (Urb &urb, Const_byte_range_ptr &src) {
					in(urb._driver_data.data,
					   { src.start, src.num_bytes });
				},

				/* produce out content */
				[&] (Urb &urb, uint32_t idx, Byte_range_ptr &dst) {
					return out_isoc(urb._driver_data.data, idx,
					                { dst.start, dst.num_bytes });
				},

				/* consume in results */
				[&] (Urb &urb, uint32_t idx, Const_byte_range_ptr &src) {
					in_isoc(urb._driver_data.data, idx,
					        { src.start, src.num_bytes });
				},

				/* complete USB request */
				[&] (Urb &urb, Usb::Tagged_packet::Return_value v) {
					using Retval = Usb::Tagged_packet::Return_value;
					genode_usb_client_ret_val_t ret;

					switch (v) {
					case Retval::NO_DEVICE: ret = NO_DEVICE; break;
					case Retval::INVALID:   ret = INVALID;   break;
					case Retval::HALT:      ret = HALT;      break;
					case Retval::OK:        ret = OK;        break;
					default:
						error("unhandled packet or timeout should not happen!");
						ret = INVALID;
					};
					complete(urb._driver_data.data, ret);
					destroy(_slab, &urb);
				});
		}

		template <typename FN>
		void with_endpoint(uint8_t index, FN const &fn)
		{
			_endpoints.for_each([&] (Endpoint &endp) {
				if (endp.address == index) fn(endp);
			});
		}

		void delete_all_urbs(auto const &complete)
		{
			if (!_iface.constructed()) return;

			_iface->dissolve_all_urbs<Urb>([&] (Urb &urb) {
				complete(urb._driver_data.data, NO_DEVICE);
				destroy(_slab, &urb);
			});
		}
};


class Device
{
	public:

		using Name  = String<64>;
		using Speed = String<32>;

		struct Urb : Usb::Device::Urb
		{
			using Descriptor   = Usb::Device::Packet_descriptor;
			using Request_type = Descriptor::Request_type;

			struct Driver_data { void * const data; } _driver_data;

			Urb(Usb::Device &device,
			    uint8_t  request,
			    uint8_t  request_type,
			    uint16_t value,
			    uint16_t index,
			    size_t   size,
			    size_t   timeout,
			    void    *opaque_data)
			:
				Usb::Device::Urb(device, request,
				                 (Request_type::access_t)request_type,
				                 value, index, size, timeout),
				_driver_data{opaque_data} { }

			bool set_interface() const;

			uint16_t index() const { return _index; }
			uint16_t value() const { return _value; }
		};

		struct Element : List_model<Element>::Element
		{
			Device &device;

			Element(Device &dev) : device(dev) {}

			bool matches(Node const &node) const {
				return device._name == node.attribute_value("name", Name()); }

			static bool type_matches(Node const &node) {
				return node.has_type("device"); }
		};

	private:

		Name                        const _name;
		Speed                       const _speed;
		::Id_space<Device>::Element const _elem;
		Usb::Connection                  &_usb;
		Env::Local_rm                    &_rm;
		Allocator                        &_alloc;
		Constructible<Usb::Device>        _device {};
		Signal_context_capability         _sigh_cap;
		void                             *_driver_data { nullptr };
		Tslab<Urb, 4096>                  _slab;

		::Id_space<::Interface> _space {};
		List_model<::Interface::Element> _iface_model {};
		Reconstructible<Element> _list_model_elem { *this };


		/**
		 * Noncopyable
		 */
		Device(Device const &);
		Device &operator = (Device const &);

		friend class Session;
		friend class Interface;

		void _delete_all_urbs(genode_usb_client_complete_t complete)
		{
			if (!_device.constructed())
				return;

			_device->dissolve_all_urbs<Urb>([&] (Urb &urb) {
				complete(urb._driver_data.data, NO_DEVICE);
				urbs_in_flight--;
				destroy(_slab, &urb);
			});

			auto iface_complete = [&] (void *opaque_data,
			                           genode_usb_client_ret_val_t r) {
				urbs_in_flight--;
				complete(opaque_data, r);
			};

			_space.for_each<::Interface>([&] (auto &iface) {
				iface.delete_all_urbs(iface_complete); });
		}

	public:

		unsigned long urbs_in_flight = 0;

		Device(Name                     &name,
		       Speed                    &speed,
		       Usb::Connection          &usb,
		       Allocator                &alloc,
		       Env::Local_rm            &rm,
		       ::Id_space<Device>       &space,
		       Signal_context_capability cap)
		:
			_name(name),
			_speed(speed),
			_elem(*this, space),
			_usb(usb),
			_rm(rm),
			_alloc(alloc),
			_sigh_cap(cap),
			_slab(alloc)
		{ }

		~Device()
		{
			_space.for_each<::Interface>([&] (auto &iface) {
				destroy(_alloc, &iface); });
		}

		void acquire()
		{
			if (_device.constructed() || !_list_model_elem.constructed())
				return;

			_device.construct(_usb, _alloc, _rm, _name);
			_device->sigh(_sigh_cap);
		}

		void with_session(auto const &fn)
		{
			if (!_list_model_elem.constructed())
				return;

			if (_device.constructed()) fn(*_device);
		}

		Name name() { return _name; }

		Usb_speed speed()
		{
			if (_speed == "low")        return GENODE_USB_SPEED_LOW;
			if (_speed == "full")       return GENODE_USB_SPEED_FULL;
			if (_speed == "high")       return GENODE_USB_SPEED_HIGH;
			if (_speed == "super")      return GENODE_USB_SPEED_SUPER;
			if (_speed == "super_plus") return GENODE_USB_SPEED_SUPER_PLUS;
			if (_speed == "super_plus_2x2")
				return GENODE_USB_SPEED_SUPER_PLUS_2X2;
			return GENODE_USB_SPEED_FULL;
		}

		Signal_context_capability sigh_cap() { return _sigh_cap; }

		genode_usb_client_dev_handle_t handle() const {
			return _elem.id().value; }

		void  driver_data(void *data) { _driver_data = data; }
		void* driver_data()           { return _driver_data; }

		Allocator &slab() { return _slab; }

		void set_interface(uint16_t index, uint16_t value);

		void update(Allocator &alloc, Node const &node)
		{
			auto with_active_config = [] (Node const &node, auto const &fn)
			{
				bool found = false;
				node.for_each_sub_node("config", [&] (Node const &config) {
					if (!found && config.attribute_value("active", false)) {
						fn(config);
						found = true;
					}
				});
				if (!found)
					fn(node);
			};

			with_active_config(node, [&] (Node const &active_config) {
				_iface_model.update_from_node(active_config,

					/* create */
					[&] (Node const &node) -> ::Interface::Element & {
						auto &iface =
							*new (alloc) ::Interface(*this, node, alloc);
						return *iface._list_model_elem;
					},

					/* destroy */
					[&] (::Interface::Element &e) {
						auto &iface = e.interface;
						iface.update(alloc, Node());
						iface._list_model_elem.destruct();

						if (!iface._iface.constructed())
							destroy(alloc, &iface);
					},

					/* update */
					[&] (::Interface::Element &e, Node const &node) {
						e.interface.update(alloc, node); }
				);
			});
		}

		void update(genode_usb_client_produce_out_t      out,
		            genode_usb_client_consume_in_t       in,
		            genode_usb_client_produce_out_isoc_t out_isoc,
		            genode_usb_client_consume_in_isoc_t  in_isoc,
		            genode_usb_client_complete_t         complete)
		{
			if (!_device.constructed())
				return;

			auto iface_complete = [&] (void *opaque_data,
			                           genode_usb_client_ret_val_t r) {
				urbs_in_flight--;
				complete(opaque_data, r);
			};

			_space.for_each<::Interface>([&] (auto &iface) {
				iface.update(out, in, out_isoc, in_isoc, iface_complete); });

			if (!_list_model_elem.constructed()) {
				_delete_all_urbs(complete);
				return;
			}

			_device->update_urbs<Urb>(

				/* produce out content */
				[&] (Urb &urb, Byte_range_ptr &dst) {
					out(urb._driver_data.data, { dst.start,
					                             dst.num_bytes });
				},

				/* consume in results */
				[&] (Urb &urb, Const_byte_range_ptr &src) {
					in(urb._driver_data.data, { src.start,
					                            src.num_bytes });
				},

				/* complete USB request */
				[&] (Urb &urb, Usb::Tagged_packet::Return_value v)
				{
					using Retval = Usb::Tagged_packet::Return_value;
					genode_usb_client_ret_val_t ret;

					switch (v) {
					case Retval::NO_DEVICE: ret = NO_DEVICE; break;
					case Retval::INVALID:   ret = INVALID;   break;
					case Retval::HALT:      ret = HALT;      break;
					case Retval::TIMEOUT:   ret = TIMEOUT;   break;
					case Retval::OK:
						ret = OK;
						if (urb.set_interface())
							set_interface(urb.index(), urb.value());
						break;
					default:
						error("unhandled packet should not happen!");
						ret = INVALID;
					};

					complete(urb._driver_data.data, ret);
					urbs_in_flight--;
					destroy(_slab, &urb);
				});
		}

		template <typename FN>
		void with_active_interfaces(FN const &fn)
		{
			_space.for_each<::Interface>([&] (auto &iface) {
				if (iface.active()) fn(iface); });
		}
};


struct Session
{
	Env                      &_env;
	Allocator                &_alloc;
	Signal_context_capability _handler_cap;
	Usb::Connection           _usb { _env, _env.ep() };
	::Id_space<Device>        _space {};

	List_model<Device::Element> _device_model {};

	Session(Env &env, Allocator &alloc, Signal_context_capability io_cap,
	        Signal_context_capability rom_cap)
	:
		_env(env), _alloc(alloc), _handler_cap(io_cap)
	{
		_usb.sigh(rom_cap);
		update_model();
	}

	~Session()
	{
		_space.for_each<Device>([&] (auto &dev) {
			destroy(_alloc, &dev); });
	}

	void update_model()
	{
		_usb.with_node([&] (Node const &node) {
			_device_model.update_from_node(node,

				/* create */
				[&] (Node const &node) -> Device::Element &
				{
					Device::Name name =
						node.attribute_value("name", Device::Name());
					Device::Speed speed =
						node.attribute_value("speed", Device::Speed());
					Device &dev = *new (_alloc)
						Device(name, speed, _usb, _alloc, _env.rm(),
						       _space, _handler_cap);
					return *dev._list_model_elem;
				},

				/* destroy */
				[&] (Device::Element &elem)
				{
					auto &dev = elem.device;

					dev.update(_alloc, Node());
					dev._list_model_elem.destruct();

					if (!dev._device.constructed())
						destroy(_alloc, &dev);
				},

				/* update */
				[&] (Device::Element &elem, Node const &node)
				{
					elem.device.update(_alloc, node);
				}
			);
		});
	}

	/*
	 * Add/delete devices for a C-API client after they got added/removed by
	 * update_model
	 */
	void update(genode_usb_client_dev_add_t add,
	            genode_usb_client_dev_del_t del)
	{
		_space.for_each<Device>([&] (Device &dev) {

			/* add new devices */
			if (!dev.driver_data() && !dev._device.constructed()) {
				dev.acquire();
				dev.driver_data(add(dev.handle(), dev.name().string(), dev.speed()));
			}

			/* delete devices marked as removed  */
			if (!dev._list_model_elem.constructed() &&
			    !dev.urbs_in_flight) {
				if (dev.driver_data()) del(dev.handle(), dev.driver_data());
				destroy(_alloc, &dev);
				return;
			}
		});
	}
};


void ::Interface::with_session(auto const &fn)
{
	if (!_list_model_elem.constructed())
		return;

	if (!_iface.constructed()) {
		_device.with_session([&] (auto &session) {
			_iface.construct(session,
			                 Usb::Interface::Index{_number, _alt_setting},
			                 _buf_size);
			_iface->sigh(_device.sigh_cap());
		});
	}

	if (_iface.constructed()) fn(*_iface);
};


::Interface::Interface(Device &device, Node const &n, Allocator &alloc)
:
	::Id_space<::Interface>::Element(*this, device._space),
	_device(device),
	_number(n.attribute_value<uint8_t>("number", 0xff)),
	_alt_setting(n.attribute_value<uint8_t>("alt_setting", 0xff)),
	_active(n.attribute_value("active", false)),
	_slab(alloc) {}


bool Device::Urb::set_interface() const
{
	return (_request == Descriptor::SET_INTERFACE) &&
	       (Request_type::R::get(_request_type) == Descriptor::IFACE) &&
	       (Request_type::T::get(_request_type) == Descriptor::STANDARD);
}


void Device::set_interface(uint16_t index, uint16_t value)
{
	_space.for_each<::Interface>([&] (auto &iface) {

		if (iface.number() != index)
			return;
		iface.active(iface.alt_setting() == value);
	});
}


static ::Session * _usb_session = nullptr;


void Genode_c_api::initialize_usb_client(Env                      &env,
                                         Allocator                &alloc,
                                         Signal_context_capability io_handler,
                                         Signal_context_capability rom_handler)
{
	static ::Session session(env, alloc, io_handler, rom_handler);
	_usb_session = &session;
};


extern "C"
void genode_usb_client_update_model(void)
{
	if (_usb_session) _usb_session->update_model();
}


extern "C"
void genode_usb_client_update_devices(genode_usb_client_dev_add_t add,
                                      genode_usb_client_dev_del_t del)
{
	if (_usb_session) _usb_session->update(add, del);
}


extern "C"
genode_usb_client_ret_val_t
genode_usb_client_device_control(genode_usb_client_dev_handle_t handle,
                                 genode_uint8_t                 request,
                                 genode_uint8_t                 request_type,
                                 genode_uint16_t                value,
                                 genode_uint16_t                index,
                                 unsigned long                  size,
                                 unsigned long                  timeout,
                                 void                          *opaque_data)
{
	try {
		if (!_usb_session)
			return NO_DEVICE;

		return _usb_session->_space.apply<Device>({ handle },
			[&] (Device &device) {
				auto result = NO_DEVICE;
				device.with_session([&] (auto &session) {
					new (device.slab())
						Device::Urb(session, request, request_type,
						            value, index, size, timeout, opaque_data);
						device.urbs_in_flight++;
						result = OK;
				});
				return result;
			},
			[] {
				return NO_DEVICE;
			});
	} catch(...) {
		return NO_MEMORY;
	}
}


extern "C"
void
genode_usb_client_device_update(genode_usb_client_produce_out_t      out,
                                genode_usb_client_consume_in_t       in,
                                genode_usb_client_produce_out_isoc_t out_isoc,
                                genode_usb_client_consume_in_isoc_t  in_isoc,
                                genode_usb_client_complete_t         complete)
{
	try {
		if (!_usb_session)
			return;

		_usb_session->_space.for_each<Device>([&] (Device &dev) {
			dev.update(out, in, out_isoc, in_isoc, complete); });
	} catch(...) { }
}


extern "C"
genode_usb_client_ret_val_t
genode_usb_client_iface_transfer(genode_usb_client_dev_handle_t handle,
                                 genode_usb_client_iface_type_t type,
                                 genode_uint8_t                 index,
                                 unsigned long                  size,
                                 void                          *opaque_data)
{
	try {
		if (!_usb_session)
			return NO_DEVICE;

		return _usb_session->_space.apply<Device>({ handle },
			[&] (Device &device) {
				auto result = NO_DEVICE;
				device.with_active_interfaces([&] (::Interface &iface) {
					iface.with_endpoint(index, [&] (Endpoint &endp) {
						iface.with_session([&] (auto &session) {
							new (iface.slab())
								::Interface::Urb(session, endp, type, size,
							                     opaque_data);
							device.urbs_in_flight++;
							result = OK;
						});
					});
				});
				return result;
			},
			[] {
				return NO_DEVICE;
			});
	} catch(...) {
		return NO_MEMORY;
	}
}
