Name:           symboldb
Version:        0.1.1
Release:        1%{?dist}
Summary:        Database of ELF, Java and Python symbols

License:        GPLv3+
URL:            https://github.com/fweimer/symboldb/
Source0:        https://github.com/fweimer/symboldb/archive/v%{version}.tar.gz

BuildRequires:	cmake
BuildRequires:	curl-devel
BuildRequires:	elfutils-devel
BuildRequires:	elfutils-libelf-devel
BuildRequires:	expat-devel
BuildRequires:	gawk
BuildRequires:	libarchive-devel
BuildRequires:	nss-devel
BuildRequires:	postgresql-contrib
BuildRequires:	postgresql-devel
BuildRequires:	postgresql-server
BuildRequires:	rpm-devel
BuildRequires:	vim-common
BuildRequires:	xmlto
BuildRequires:	zlib-devel
BuildRequires:  python
BuildRequires:  python3

Requires:       python
Requires:  	python3

%description
symboldb extracts definitions of and references to ELF symbols,
Java classes, and Python imports from binary RPM packages and
loads them into a PostgreSQL database.

%prep
%setup -q
%autosetup -p1

%build
mkdir build
cd build
cmake \
        -DCMAKE_VERBOSE_MAKEFILE:BOOL=ON \
        -DCMAKE_INSTALL_PREFIX:PATH=%{_prefix} \
        -DINCLUDE_INSTALL_DIR:PATH=%{_includedir} \
        -DLIB_INSTALL_DIR:PATH=%{_libdir} \
        -DSYSCONF_INSTALL_DIR:PATH=%{_sysconfdir} \
        -DSHARE_INSTALL_PREFIX:PATH=%{_datadir} \
	..
make %{?_smp_mflags}
cd ..
build/runtests


%install
rm -rf $RPM_BUILD_ROOT
cd build
%make_install


%files
/usr/bin/symboldb
/usr/bin/pgtestshell
%doc
/usr/share/man/man1/symboldb.1.gz
%changelog
* Thu Jul 18 2013 Florian Weimer <fweimer@redhat.com> - 0.1.1-1
- Initial RPM release
