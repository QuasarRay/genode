proc prepare_preboot_file { } {

	set build_dir "build_x86_32_preboot"

	if {![file exists $build_dir]} {
		exec ../../tool/create_builddir x86_32 BUILD_DIR=$build_dir

		set fc [open $build_dir/etc/build.conf a]
		puts $fc { REPOSITORIES += $(GENODE_DIR)/repos/preboot }
		puts $fc { REPOSITORIES += $(GENODE_DIR)/repos/gems }
		close $fc
	}

	exec make -C $build_dir boloa

	if {![file exists $build_dir/bin/boloa]} {
		puts "Genode's boot loader is missing"
		exit -1
	}

	exec cp $build_dir/bin/boloa [run_dir]/boot/.
}
