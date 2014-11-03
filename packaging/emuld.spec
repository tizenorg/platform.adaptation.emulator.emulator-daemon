Name: emuld
Version: 0.8.0
Release: 0
Summary: Emulator daemon
License: Apache-2.0
Source0: %{name}-%{version}.tar.gz
Group: SDK/Other
%if "%{?tizen_profile_name}" == "mobile"
Source1001: packaging/emuld_mobile.manifest
%elseif "%{?tizen_profile_name}" == "wearable"
Source1001: packaging/emuld_wearable.manifest
%endif

BuildRequires: cmake
BuildRequires: pkgconfig(vconf)
BuildRequires: pkgconfig(deviced)
BuildRequires: pkgconfig(dlog)

%if "%{?tizen_profile_name}" == "wearable"
Requires: context-manager
%endif

%description
A emulator daemon is used for communication between guest and host

%prep
%setup -q

%if "%{?tizen_profile_name}" == "mobile"
export CFLAGS+=" -DMOBILE"
%elseif "%{?tizen_profile_name}" == "wearable"
export CFLAGS+=" -DWEARABLE"
%endif

cmake . -DCMAKE_INSTALL_PREFIX=%{_prefix}

%build

make

%install
rm -rf %{buildroot}

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

%if "%{?tizen_profile_name}" == "mobile"
%manifest packaging/emuld_mobile.manifest
%elseif "%{?tizen_profile_name}" == "wearable"
%manifest packaging/emuld_wearable.manifest
%endif

%{_prefix}/bin/emuld
/usr/share/license/%{name}
/usr/lib/systemd/system/emuld.service
/usr/lib/systemd/system/emulator.target.wants/emuld.service

%changelog
