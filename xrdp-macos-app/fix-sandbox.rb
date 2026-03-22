#!/usr/bin/env ruby
require 'xcodeproj'

project_path = 'xrdp.xcodeproj'
project = Xcodeproj::Project.open(project_path)

target = project.targets.first
patch_phase = target.shell_script_build_phases.find { |phase| phase.name == "Patch Configs" }

if patch_phase
  patch_phase.input_paths = ['$(SRCROOT)/patch-configs.sh']
  puts "✅ Added patch-configs.sh to input paths"
  project.save
  puts "✅ Project saved"
else
  puts "❌ Patch Configs phase not found"
end
