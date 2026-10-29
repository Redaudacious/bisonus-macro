cmake_minimum_required(VERSION 3.22)
project(bisonus-macro VERSION 0.7.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
  set(CMAKE_INSTALL_PREFIX /usr CACHE PATH "Pachetul .deb instalează sub /usr" FORCE)
endif()

find_package(ECM 6.0 REQUIRED NO_MODULE)
list(APPEND CMAKE_MODULE_PATH ${ECM_MODULE_PATH})
include(KDEInstallDirs6)
# i18n() caută textele în traducerea aplicației, po/ro/bisonus-macro.po
add_definitions(-DTRANSLATION_DOMAIN=\"bisonus-macro\")

find_package(Qt6 6.6 REQUIRED COMPONENTS Widgets Test DBus)
find_package(KF6 6.0 REQUIRED COMPONENTS GlobalAccel XmlGui Config DBusAddons I18n ColorScheme StatusNotifierItem WidgetsAddons)

# tastatura virtuală: EIS-ul lui KWin, prin libei
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBEI REQUIRED IMPORTED_TARGET libei-1.0)

include(CTest)

# nucleul, fără interfață: tastele, fișierul de macro-uri și motorul
add_library(nucleu STATIC src/taste.cpp src/limbaj.cpp src/macrouri.cpp src/motor.cpp src/resincronizare.cpp src/activare.cpp src/editor.cpp)
target_include_directories(nucleu PUBLIC src)
target_link_libraries(nucleu PUBLIC Qt6::Gui KF6::I18n)

if(BUILD_TESTING)
  add_executable(test_taste probe/test_taste.cpp)
  target_link_libraries(test_taste PRIVATE nucleu Qt6::Test)
  add_test(NAME test_taste COMMAND test_taste)
  set_tests_properties(test_taste PROPERTIES ENVIRONMENT QT_QPA_PLATFORM=offscreen)
endif()

if(BUILD_TESTING)
  add_executable(test_macrouri probe/test_macrouri.cpp)
  target_compile_definitions(test_macrouri PRIVATE MACROS_JSON="${CMAKE_SOURCE_DIR}/date/macros.json"
                                                   BLOODLINES_JSON="${CMAKE_SOURCE_DIR}/date/templates/bloodlines.json")
  target_link_libraries(test_macrouri PRIVATE interfata Qt6::Test)
  add_test(NAME test_macrouri COMMAND test_macrouri)
  set_tests_properties(test_macrouri PROPERTIES ENVIRONMENT QT_QPA_PLATFORM=offscreen)
endif()

if(BUILD_TESTING)
  add_executable(test_motor probe/test_motor.cpp)
  target_link_libraries(test_motor PRIVATE nucleu Qt6::Test)
  add_test(NAME test_motor COMMAND test_motor)
  set_tests_properties(test_motor PROPERTIES ENVIRONMENT QT_QPA_PLATFORM=offscreen)
endif()

if(BUILD_TESTING)
  add_executable(test_resincronizare probe/test_resincronizare.cpp)
  target_link_libraries(test_resincronizare PRIVATE nucleu Qt6::Test)
  add_test(NAME test_resincronizare COMMAND test_resincronizare)
  set_tests_properties(test_resincronizare PROPERTIES ENVIRONMENT QT_QPA_PLATFORM=offscreen)
endif()

if(BUILD_TESTING)
  add_executable(test_activare probe/test_activare.cpp)
  target_link_libraries(test_activare PRIVATE nucleu Qt6::Test)
  add_test(NAME test_activare COMMAND test_activare)
  set_tests_properties(test_activare PROPERTIES ENVIRONMENT QT_QPA_PLATFORM=offscreen)
endif()

if(BUILD_TESTING)
  add_executable(test_limbaj probe/test_limbaj.cpp)
  target_link_libraries(test_limbaj PRIVATE nucleu Qt6::Test)
  add_test(NAME test_limbaj COMMAND test_limbaj)
  set_tests_properties(test_limbaj PROPERTIES ENVIRONMENT QT_QPA_PLATFORM=offscreen)
endif()

# interfața: setările și panoul
add_library(interfata STATIC src/setari.cpp src/regulakwin.cpp src/kwinscript.cpp src/export.cpp)
target_link_libraries(interfata PUBLIC nucleu Qt6::Widgets KF6::GlobalAccel KF6::XmlGui KF6::WidgetsAddons KF6::ConfigCore KF6::ColorScheme Qt6::DBus)

if(BUILD_TESTING)
  add_executable(test_setari probe/test_setari.cpp)
  target_link_libraries(test_setari PRIVATE interfata Qt6::Test)
  target_compile_definitions(test_setari PRIVATE CAPTURI_SH="${CMAKE_SOURCE_DIR}/teste/capturi-ajutor.sh")
  add_test(NAME test_setari COMMAND test_setari)
  set_tests_properties(test_setari PROPERTIES ENVIRONMENT QT_QPA_PLATFORM=offscreen)
endif()

if(BUILD_TESTING)
  add_executable(test_fereastra probe/test_fereastra.cpp)
  target_link_libraries(test_fereastra PRIVATE interfata Qt6::Test)
  target_compile_definitions(test_fereastra PRIVATE SABLOANE="${CMAKE_SOURCE_DIR}/date/templates")
  add_test(NAME test_fereastra COMMAND test_fereastra)
  set_tests_properties(test_fereastra PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QPA_PLATFORMTHEME=kde;XDG_CURRENT_DESKTOP=KDE")
endif()

if(BUILD_TESTING)
  add_executable(test_regulakwin probe/test_regulakwin.cpp)
  target_link_libraries(test_regulakwin PRIVATE interfata Qt6::Test)
  add_test(NAME test_regulakwin COMMAND test_regulakwin)
  set_tests_properties(test_regulakwin PROPERTIES ENVIRONMENT QT_QPA_PLATFORM=offscreen)
endif()

if(BUILD_TESTING)
  add_executable(test_panou probe/test_panou.cpp)
  target_link_libraries(test_panou PRIVATE interfata Qt6::Test)
  add_test(NAME test_panou COMMAND test_panou)
  set_tests_properties(test_panou PROPERTIES ENVIRONMENT QT_QPA_PLATFORM=offscreen)
endif()

if(BUILD_TESTING)
  add_executable(test_peste probe/test_peste.cpp)
  target_link_libraries(test_peste PRIVATE interfata Qt6::Test)
  add_test(NAME test_peste COMMAND test_peste)
  set_tests_properties(test_peste PROPERTIES ENVIRONMENT QT_QPA_PLATFORM=offscreen)
endif()
if(BUILD_TESTING)
  add_executable(test_editor probe/test_editor.cpp)
  target_link_libraries(test_editor PRIVATE interfata Qt6::Test)
  add_test(NAME test_editor COMMAND test_editor)
  set_tests_properties(test_editor PROPERTIES ENVIRONMENT QT_QPA_PLATFORM=offscreen)
endif()
if(BUILD_TESTING)
  add_executable(test_pornire probe/test_pornire.cpp)
  target_link_libraries(test_pornire PRIVATE interfata Qt6::Test)
  add_test(NAME test_pornire COMMAND test_pornire)
  set_tests_properties(test_pornire PROPERTIES ENVIRONMENT QT_QPA_PLATFORM=offscreen)
endif()
if(BUILD_TESTING)
  add_executable(test_export probe/test_export.cpp)
  target_link_libraries(test_export PRIVATE interfata Qt6::Test)
  add_test(NAME test_export COMMAND test_export)
  set_tests_properties(test_export PROPERTIES ENVIRONMENT QT_QPA_PLATFORM=offscreen)
endif()
if(BUILD_TESTING)
  add_executable(test_noutati probe/test_noutati.cpp)
  target_link_libraries(test_noutati PRIVATE interfata Qt6::Test)
  target_compile_definitions(test_noutati PRIVATE CHANGELOG_MD="${CMAKE_SOURCE_DIR}/CHANGELOG.md" VERSIUNE="${PROJECT_VERSION}")
  add_test(NAME test_noutati COMMAND test_noutati)
  set_tests_properties(test_noutati PROPERTIES ENVIRONMENT QT_QPA_PLATFORM=offscreen)
endif()
if(BUILD_TESTING)
  add_executable(test_ajutor probe/test_ajutor.cpp)
  target_link_libraries(test_ajutor PRIVATE interfata Qt6::Test)
  add_test(NAME test_ajutor COMMAND test_ajutor)
  set_tests_properties(test_ajutor PROPERTIES ENVIRONMENT QT_QPA_PLATFORM=offscreen)
endif()

target_sources(interfata PRIVATE src/panou.cpp src/ajutor.cpp src/editorscript.cpp src/pornire.cpp src/noutati.cpp)

add_executable(bisonus-macro src/main.cpp src/tastatura.cpp src/scrieresigura.cpp)
target_link_libraries(bisonus-macro PRIVATE interfata Qt6::DBus PkgConfig::LIBEI)
target_link_libraries(bisonus-macro PRIVATE KF6::DBusAddons KF6::StatusNotifierItem)
target_compile_definitions(bisonus-macro PRIVATE VERSIUNE="${PROJECT_VERSION}") # --version
qt_add_resources(bisonus-macro changelog PREFIX "/" BASE "${CMAKE_SOURCE_DIR}" FILES CHANGELOG.md) # „What's new”, fără internet

if(BUILD_TESTING)
  # fereastra cu focusul din probele de pe desktop, cu receptorul D-Bus al scripturilor KWin de probă
  add_executable(fereastra-tinta probe/fereastra-tinta.cpp)
  target_link_libraries(fereastra-tinta PRIVATE Qt6::Widgets Qt6::DBus)
endif()

if(BUILD_TESTING)
  # programul de probă al recenziilor: compilează și rulează un script fără taste adevărate
  add_executable(compileaza probe/compileaza.cpp)
  target_link_libraries(compileaza PRIVATE nucleu)
endif()

if(BUILD_TESTING)
  # componenta veche de probă din teste/proba-componenta-veche.sh
  add_executable(componenta-veche probe/componenta-veche.cpp)
  target_link_libraries(componenta-veche PRIVATE Qt6::Widgets KF6::GlobalAccel)
endif()

if(BUILD_TESTING)
  # tastatura fizică din teste/proba-panou.sh --tine-w
  add_executable(tastatura-proba probe/tastatura-proba.cpp)
endif()

install(TARGETS bisonus-macro DESTINATION ${KDE_INSTALL_BINDIR})
install(FILES date/bisonus-macro.desktop DESTINATION ${KDE_INSTALL_APPDIR})
# AppStream: numele și descrierea din Discover, în engleză și în română. Fișierul .desktop își păstrează numele:
# el e app_id-ul ferestrei, de care pot depinde acordul portalului și tastele globale.
install(FILES date/io.github.redaudacious.bisonus_macro.metainfo.xml DESTINATION ${KDE_INSTALL_METAINFODIR})
install(FILES date/bisonus-macro.svg DESTINATION ${KDE_INSTALL_ICONDIR}/hicolor/scalable/apps)
# iconița tăvii cât macro-urile sunt oprite
install(FILES date/bisonus-macro-off.svg DESTINATION ${KDE_INSTALL_ICONDIR}/hicolor/scalable/apps)
install(FILES date/macros.json DESTINATION ${KDE_INSTALL_DATADIR}/bisonus-macro)
# Template-urile (hotărârea 41). bloodlines.json ține arborele Bloodlines, cu scripturile lui. Pachetul pornește fără
# macro-uri: omul îl încarcă din pagina goală sau din „New from template…”.
install(FILES date/templates/bloodlines.json date/templates/empty.json
        DESTINATION ${KDE_INSTALL_DATADIR}/bisonus-macro/templates)
# Help-ul: paginile în engleză și în română și capturile lor (hotărârea 42)
install(DIRECTORY date/help DESTINATION ${KDE_INSTALL_DATADIR}/bisonus-macro)
# KColorSchemeManager caută schemele numai în color-schemes/: tema se vede și în restul KDE-ului
install(FILES date/Bloodlines.colors DESTINATION ${KDE_INSTALL_DATADIR}/color-schemes)

# licența, unde o caută Debian; pachetul .rpm o pune și în licenses/, cu %license
install(FILES LICENSE DESTINATION ${KDE_INSTALL_DATAROOTDIR}/doc/bisonus-macro RENAME copyright)

# traducerea română: po/ro/bisonus-macro.po devine share/locale/ro/LC_MESSAGES/bisonus-macro.mo
ki18n_install(po)

set(CPACK_GENERATOR DEB)
set(CPACK_PACKAGE_NAME bisonus-macro)
set(CPACK_PACKAGE_CONTACT "Rosu Mihai")
# Descrierea pachetului e în engleză: managerul de pachete o arată la fel în orice limbă a sistemului.
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Keyboard and mouse macros for KDE Plasma 6 on Wayland")
set(CPACK_PACKAGE_DESCRIPTION "BisonusMacro runs keyboard and mouse macros on Linux, on KDE Plasma 6 with Wayland. The keys and the mouse go through the KDE RemoteDesktop portal.")
set(CPACK_PACKAGING_INSTALL_PREFIX /usr)
set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
set(CPACK_DEBIAN_PACKAGE_SECTION utils)
# pachetul vechi, de dinaintea numelui BisonusMacro: apt îl scoate la instalare
set(CPACK_DEBIAN_PACKAGE_CONFLICTS bloodlines-macro)
set(CPACK_DEBIAN_PACKAGE_REPLACES bloodlines-macro)
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON) # dpkg-shlibdeps pune singur dependențele de biblioteci
# Portalul și KWin nu sunt biblioteci, deci dpkg-shlibdeps nu le găsește. CPack adaugă lista la ce găsește el.
set(CPACK_DEBIAN_PACKAGE_DEPENDS "xdg-desktop-portal-kde, kwin-wayland")
set(CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA ${CMAKE_SOURCE_DIR}/packaging/debian/postinst)
set(CPACK_DEBIAN_PACKAGE_CONTROL_STRICT_PERMISSION ON)
include(CPack)
