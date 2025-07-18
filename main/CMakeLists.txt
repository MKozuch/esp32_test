set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED true)

idf_component_register(SRCS "hello_world_main.cpp"
        PRIV_REQUIRES spi_flash
        PRIV_REQUIRES driver
        REQUIRES cxx
        INCLUDE_DIRS "")