Summary: Library for handling ISS MIDAS DAQ data files with root
Name: libANISS
%define version 20180615
%define release %(root-config --version |sed -e 's*/*.*').1
Version: %{version}
Release: %{release}%{dist}
Vendor: Joonas Konki
License: MIT
Group: Applications/Analysis
Source: /usr/src/redhat/SOURCES/libANISS.tar.gz
Provides: libANISS.so
BuildRequires: root-core
Requires: root-core
BuildRoot: /tmp/package_%{name}-%{version}.%{release}

%description
libANISS is a library for processing ISS data files using root.

%prep
%setup

%build
make %{?_smp_mflags}

%install
make DESTDIR="$RPM_BUILD_ROOT" install
find $RPM_BUILD_ROOT -not -type d | sed -e "s*$RPM_BUILD_ROOT**" >> rpmfilelist.txt

%post -p /sbin/ldconfig

%postun -p /sbin/ldconfig

%files -f rpmfilelist.txt
%defattr(-,root,root,-)
%doc doc/libANISS.tex doc/libANISS.pdf
%doc examples/*.C

