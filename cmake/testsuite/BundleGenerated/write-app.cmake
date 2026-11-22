# The writer of the web app of the bundle com.example.written: replaces the directory APP_DIR with one that holds
# app-<version>.js, the version being the content of VERSION_FILE, so that each version has other file names.

file(STRINGS "${VERSION_FILE}" version)
file(REMOVE_RECURSE "${APP_DIR}")
file(WRITE "${APP_DIR}/app-${version}.js" "app ${version}\n")
