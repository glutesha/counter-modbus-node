# NOTE: This kind of mocking currently works on Linux targets only.
#       On Espressif chips, too many dependencies are missing at the moment.

idf_component_mock(INCLUDE_DIRS .
                    REQUIRES cmock test_common
                    MOCK_HEADER_FILES test_mbm_object.h)



