Name: emuld
Version: 0.4.2
Release: 0
Summary: Emulator daemon
License: Apache-2.0
Source0: %{name}-%{version}.tar.gz
Group: SDK/Other
Source1001: packaging/emuld.manifest
BuildRequires: cmake
BuildRequires: pkgconfig(vconf)
BuildRequires: pkgconfig(dlog)
BuildRequires: pkgconfig(pmapi)

%description
A emulator daemon is used for communication between guest and host

%prep
%setup -q

%build
export LDFLAGS+="-Wl,--rpath=%{_prefix}/lib -Wl,--as-needed"

LDFLAGS="$LDFLAGS" cmake . -DCMAKE_INSTALL_PREFIX=%{_prefix}

make

%install
if [ ! -d %{buildroot}/usr/lib/systemd/system/emulator.target.wants ]; then
    mkdir -p %{buildroot}/usr/lib/systemd/system/emulator.target.wants
fi

cp emuld.service %{buildroot}/usr/lib/systemd/system/.
ln -s ../emuld.service %{buildroot}/usr/lib/systemd/system/emulator.target.wants/emuld.service

mkdir -p %{buildroot}/usr/share/license
cp LICENSE %{buildroot}/usr/share/license/%{name}

%make_install

%clean
make clean
rm -rf CMakeCache.txt
rm -rf CMakeFiles
rm -rf cmake_install.cmake
rm -rf Makefile
rm -rf install_manifest.txt

%post
chmod 770 %{_prefix}/bin/emuld

%files
%defattr(-,root,root,-)
%{_prefix}/bin/emuld
/usr/share/license/%{name}
/usr/lib/systemd/system/emuld.service
/usr/lib/systemd/system/emulator.target.wants/emuld.service

%changelog
