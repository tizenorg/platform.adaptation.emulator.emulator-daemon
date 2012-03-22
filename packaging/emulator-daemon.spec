#git:/slp/pkgs/e/emulator-daemon
Name: emulator-daemon
Version: 0.2.4
Release: 1
Summary: emuld is used for communication emulator between and ide.
License: Apache
Source0: %{name}-%{version}.tar.gz
BuildArch: i386
ExclusiveArch: %{ix86}
BuildRequires: cmake

%description

%prep
%setup -q

%build
cmake . -DCMAKE_INSTALL_PREFIX=%{_prefix}

make

%install
rm -rf %{buildroot}
%make_install

%clean
make clean
rm -rf CMakeCache.txt
rm -rf CMakeFiles
rm -rf cmake_install.cmake
rm -rf Makefile
rm -rf install_manifes.txt

%post
chmod 777 /usr/bin/emuld

%postun

%files
%defattr(-,root,root,-)
%{_prefix}/bin/emuld

%changelog
