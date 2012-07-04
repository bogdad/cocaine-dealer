#properly install perl module to destination directory

SET(MODULE_TMP_DIR /tmp/perl/modules/cocaine_dealer)

EXECUTE_PROCESS(
	COMMAND ${CMAKE_COMMAND} -E chdir ${MODULE_TMP_DIR} perl -MExtUtils::Install -e install_default Cocaine/Dealer
	COMMAND ${CMAKE_COMMAND} -E remove_directory ${MODULE_TMP_DIR}
	)