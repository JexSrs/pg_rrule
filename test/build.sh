cd /app/libical && \
rm -rf build && \
mkdir build && \
cd build && \
cmake \
    -DSTATIC_ONLY=True \
    -DGOBJECT_INTROSPECTION=False \
    -DLIBICAL_BUILD_TESTING=False \
    -DICAL_BUILD_DOCS=False \
    -DICAL_GLIB=False \
    -DCMAKE_CXX_FLAGS="-fPIC -std=c++11" \
    -DCMAKE_C_FLAGS="-fPIC" \
    -DCMAKE_DISABLE_FIND_PACKAGE_ICU=TRUE \
    -DLIBICAL_JAVA_BINDINGS=FALSE \
    .. && \
make && /
cd /app && \
rm -rf build && \
mkdir build && \
cd build && \
cmake .. && \
make