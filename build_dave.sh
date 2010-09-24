#!/bin/sh


g++ coregister.cc tracks.cc match.cc io.cc display.cc weights.cpp  -o coregister -I. -I/irg/packages/x86_64_linux_gcc4.1/boost/include -I/hosts/sudoko/home/dtjackso/projects/vision_workbench/build/include -I/hosts/sudoko/home/dtjackso/projects/vision_workbench/src/ -I/hosts/sudoko/home/dtjackso/projects/vision_workbench/build/x86_64_linux_gcc4.1/bin -L/irg/packages/x86_64_linux_gcc4.1/boost/lib  -L/opt/local/lib -L/hosts/sudoko/home/dtjackso/projects/vision_workbench/build/x86_64_linux_gcc4.1/lib -lvwCore -lvwMath -lvwImage -lboost_thread-mt -lboost_program_options-mt -lboost_filesystem-mt -lboost_system-mt -lvwFileIO -lvwCartography -lvwPhotometry -g

#commented out as last version
#g++ coregister.cc match.cc io.cc -o coregister -I. -I/irg/packages/x86_64_linux_gcc4.1/boost/include -I/hosts/sudoko/home/dtjackso/projects/vision_workbench/build/include -I/hosts/sudoko/home/dtjackso/projects/vision_workbench/src/ -I/hosts/sudoko/home/dtjackso/projects/vision_workbench/build/x86_64_linux_gcc4.1/bin -L/irg/packages/x86_64_linux_gcc4.1/boost/lib  -L/opt/local/lib -L/hosts/sudoko/home/dtjackso/projects/vision_workbench/build/x86_64_linux_gcc4.1/lib -lvwCore -lvwMath -lvwImage -lboost_thread-mt -lboost_program_options-mt -lboost_filesystem-mt -lboost_system-mt -lvwFileIO -lvwCartography -lvwPhotometry -g


# below works - with many -lbloost* removed!
#g++ coregister.cc match.cc io.cc -o coregister -I. -I/irg/packages/x86_64_linux_gcc4.1/boost/include -I/hosts/sudoko/home/dtjackso/projects/vision_workbench/build/include -I/hosts/sudoko/home/dtjackso/projects/vision_workbench/src/ -I/hosts/sudoko/home/dtjackso/projects/vision_workbench/build/x86_64_linux_gcc4.1/bin -L/irg/packages/x86_64_linux_gcc4.1/boost/lib  -L/opt/local/lib -L/hosts/sudoko/home/dtjackso/projects/vision_workbench/build/x86_64_linux_gcc4.1/lib -lvwCore -lvwMath -lvwImage  -lvwFileIO -lvwCartography -lvwPhotometry -g

#g++ coregister.cc match.cc io.cc -o coregister -I. -I/irg/packages/x86_64_linux_gcc4.1/boost/include -I/hosts/sudoko/home/dtjackso/projects/vision_workbench/build/include -I/hosts/sudoko/home/dtjackso/projects/vision_workbench/src/ -I/hosts/sudoko/home/dtjackso/projects/vision_workbench/build/x86_64_linux_gcc4.1/bin -L/irg/packages/x86_64_linux_gcc4.1/boost/lib  -L/opt/local/lib -L/hosts/sudoko/home/dtjackso/projects/vision_workbench/build/x86_64_linux_gcc4.1/lib -lvwCore -lvwMath -lvwImage -lboost_thread-mt -lboost_program_options-mt -lboost_filesystem-mt -lboost_system-mt -lvwFileIO -lvwCartography -lvwPhotometry -g

#Currently I pulled out '-lcv -lcvaux -lcxcor -lhighgui -framework Veclib' from the very end of the script
