Name:           bisonus-macro
Version:        0.7.0
Release:        1%{?dist}
Summary:        Keyboard and mouse macros for KDE Plasma 6 on Wayland
License:        GPL-3.0-or-later
URL:            https://github.com/Redaudacious/bisonus-macro
Source0:        %{name}-%{version}.tar.gz

BuildRequires:  cmake
BuildRequires:  gcc-c++
BuildRequires:  make
BuildRequires:  extra-cmake-modules
BuildRequires:  qt6-qtbase-devel
BuildRequires:  kf6-kglobalaccel-devel
BuildRequires:  kf6-kxmlgui-devel
BuildRequires:  kf6-kconfig-devel
BuildRequires:  kf6-kdbusaddons-devel
BuildRequires:  kf6-ki18n-devel
BuildRequires:  kf6-kcolorscheme-devel
BuildRequires:  kf6-kstatusnotifieritem-devel
BuildRequires:  kf6-kwidgetsaddons-devel
BuildRequires:  libei-devel
BuildRequires:  gettext
Requires:       xdg-desktop-portal-kde
Requires:       kwin
Requires:       hicolor-icon-theme

%description
BisonusMacro runs keyboard and mouse macros on Linux, on KDE Plasma 6
with Wayland. The keys and the mouse go through the KDE RemoteDesktop portal.

%prep
%autosetup

%build
%cmake -DBUILD_TESTING=OFF
%cmake_build

%install
%cmake_install

%files
%license LICENSE
%{_bindir}/bisonus-macro
%{_datadir}/applications/bisonus-macro.desktop
%{_metainfodir}/io.github.redaudacious.bisonus_macro.metainfo.xml
%{_datadir}/icons/hicolor/scalable/apps/bisonus-macro.svg
%{_datadir}/icons/hicolor/scalable/apps/bisonus-macro-off.svg
%{_datadir}/bisonus-macro/
%dir %{_datadir}/color-schemes
%{_datadir}/color-schemes/Bloodlines.colors
%{_datadir}/locale/ro/LC_MESSAGES/bisonus-macro.mo
%{_docdir}/%{name}/

%changelog
* Thu Oct 29 2026 Rosu Mihai - 0.7.0-1
- Scripting language with an editor, a stop key and an empty first start
- KDE asks once for keyboard and mouse access (again after the update to 0.7.0)

* Wed Oct 28 2026 Rosu Mihai - 0.6.4-1
- First RPM package
