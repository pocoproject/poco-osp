# A superproject that finds an installed POCO and adds poco-osp (OSP_SOURCE_DIR) after it.

cmake_minimum_required(VERSION 3.26)
project(InstalledPocoSuperproject LANGUAGES C CXX)

find_package(Poco REQUIRED COMPONENTS Foundation)
add_subdirectory(${OSP_SOURCE_DIR} poco-osp)
