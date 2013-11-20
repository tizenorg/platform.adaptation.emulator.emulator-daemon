#git:/slp/pkgs/e/emulator-daemon
Name: emuld
Version: 0.2.30
Release: 1
Summary: emuld is used for communication emulator between and ide.
License: Apache
Source0: %{name}-%{version}.tar.gz
Source1001: packaging/emuld.manifest
BuildArch: i386
ExclusiveArch: %{ix86}
BuildRequires: cmake
BuildRequires:  pkgconfig(vconf)

%description

%prep
%setup -q

%build
export LDFLAGS+="-Wl,--rpath=%{_prefix}/lib -Wl,--as-needed"    
    
LDFLAGS="$LDFLAGS" cmake . -DCMAKE_INSTALL_PREFIX=%{_prefix}

make

%install
rm -rf %{buildroot}

#for systemd
if [ ! -d %{buildroot}/usr/lib/systemd/system/emulator.target.wants ]; then
    mkdir -p %{buildroot}/usr/lib/systemd/system/emulator.target.wants
fi
cp emuld.service %{buildroot}/usr/lib/systemd/system/.
ln -s ../emuld.service %{buildroot}/usr/lib/systemd/system/emulator.target.wants/emuld.service

#for legacy init
if [ ! -d %{buildroot}/etc/init.d ]; then
    mkdir -p %{buildroot}/etc/init.d
fi
cp emuld %{buildroot}/etc/init.d/.
if [ ! -d %{buildroot}/etc/rc.d/rc3.d ]; then
    mkdir -p %{buildroot}/etc/rc.d/rc3.d
fi
ln -s /etc/init.d/emuld %{buildroot}/etc/rc.d/rc3.d/S04emuld

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
mkdir -p /opt/nfc

%postun

%files
%defattr(-,root,root,-)
%{_prefix}/bin/emuld
/usr/lib/systemd/system/emuld.service
/usr/lib/systemd/system/emulator.target.wants/emuld.service
/etc/init.d/emuld
/etc/rc.d/rc3.d/S04emuld

%changelog
