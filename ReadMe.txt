NachOS Folder info

- [Folder]	code : NachOS kernel & User program

- [Folder]	coff2noff、usr : cross-compiler (for MIPS ISA)
- [File]	mips-decstation.linux-xgcc.gz : After Unzip == [Folder] usr

- [Folder]	c+example : ???


1 Environment

- Virtual OS: Red Hat Enterprise Linux 9 (RHEL 9 x86_64)
- Virtual Machine: Parallels Desktop 18.1.1


2 Build NachOS

- [File] NachOS-4.0_MP1.tar.gz
  - Unzip it.
  - Put the folder in /home/

- Set up the dependency
  - sudo yum install gcc
  - sudo yum install g++
  - sudo yum install libstdc++.i686

  - sudo cp -r /home/<username>/nachos-4.0/usr/local/ /usr 
    (If it works, you'll see the nachos folder in /usr/local )

  - sudo chmod -R 777 /home/vbox/NachOS-4.0_MP1/usr/local

  - sudo chmod -R 777 /home/vbox/NachOS-4.0_MP1/coff2noff

  - sudo chmod -R 777 /usr/local/nachos

- Compile the NachOS kernel

  - cd /home/<username>/NachOS-4.0_MP1/code/build.linux
  - make clean
  - make depend <- It's important!
  - make


- Test NachOS

  - ./nachos –u
  - ./nachos –e ../test/halt


3 Build & Run Test Programs

- Compile the test program (ex. halt.c)

  - cd /home/<username>/NachOS-4.0_MP1/code/test
  - make clean
  - make halt


- Test NachOS
  - /home/vbox/NachOS-4.0_MP1/code/build.linux/nachos -e ../test/halt


4 Some errors

- If the Terminal shows something wrong about list.cc
  - Revising sth like : isEmpty() to this->isEmpty
- If /usr/bin/ld: cannot find libstdc++
  - sudo yum install libstdc++.i686
- If it shows "... -I- ... -iquote instead"
  - It doesn't matter
- make: ../../usr/local/nachos/bin/decstation-ultrix-gcc: ...
  - sudo chmod -R 777 /home/vbox/NachOS-4.0_MP1/usr/local
- decstation-ultrix-gcc: installation problem, cannot exec `cc1': ?????????
  - sudo chmod -R 777 /home/vbox/NachOS-4.0_MP1/coff2noff
- make: ../../coff2noff/coff2noff.x86Linux:...
  - sudo chmod -R 777 /usr/local/nachos

