CXX      = g++
CXXFLAGS = -std=c++17 -Wall -g
STATIC_CXXFLAGS = -std=c++17 -Wall -O2
.RECIPEPREFIX := >

GRPC_SRC    = external/grpc
GRPC_BUILD  = external/grpc/cmake/build
PROTO_BUILD = $(GRPC_BUILD)/third_party/protobuf
ABSL_BUILD  = $(GRPC_BUILD)/third_party/abseil-cpp/absl

INCLUDES = \
    -I. \
    -Iexternal/json/include \
    -I$(GRPC_SRC)/include \
    -I$(GRPC_SRC)/third_party/abseil-cpp \
    -I$(GRPC_SRC)/third_party/protobuf/src \
    -I$(GRPC_SRC)/third_party/protobuf/third_party/utf8_range

LIBS_GRPC = \
    -Wl,--start-group \
    $(GRPC_BUILD)/libgrpc++.a \
    $(GRPC_BUILD)/libgrpc.a \
    $(GRPC_BUILD)/libgpr.a \
    $(GRPC_BUILD)/libaddress_sorting.a \
    $(PROTO_BUILD)/libprotobuf.a \
    $(PROTO_BUILD)/libupb.a \
    $(wildcard $(GRPC_BUILD)/libupb_*_lib.a) \
    $(PROTO_BUILD)/third_party/utf8_range/libutf8_range.a \
    $(PROTO_BUILD)/third_party/utf8_range/libutf8_validity.a \
    $(wildcard $(ABSL_BUILD)/*/libabsl_*.a) \
    $(GRPC_BUILD)/third_party/re2/libre2.a \
    $(GRPC_BUILD)/third_party/cares/cares/src/lib/libcares.a \
    $(GRPC_BUILD)/third_party/zlib/libz.a \
    $(GRPC_BUILD)/third_party/boringssl-with-bazel/libssl.a \
    $(GRPC_BUILD)/third_party/boringssl-with-bazel/libcrypto.a \
    -Wl,--end-group \
    -lpthread \
    -ldl \
    -lsystemd

GENERATED_SRCS = \
    generated/ecg.grpc.pb.cc \
    generated/ecg.pb.cc

GENERATED_OBJS = \
    obj/ecg.grpc.pb.o \
    obj/ecg.pb.o \
    obj/auth.o

APP_OBJS = \
    obj/streamer.o \
    obj/ecg_producer.o

CURL_CFLAGS = $(shell pkg-config --cflags libcurl)
CURL_LIBS = $(shell pkg-config --libs libcurl)

TARGET = ecg_producer
STATICLIB = libs/libecg_producer.a

STATIC_OBJS = \
    obj/static/ecg.grpc.pb.o \
    obj/static/ecg.pb.o \
    obj/static/auth.o \
    obj/static/streamer.o \
    obj/static/ecg_producer.o

all: $(TARGET)

staticlib: libs $(STATICLIB)

obj/%.o: generated/%.cc init_bin
>$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

obj/auth.o: init_bin auth/auth.cpp auth/auth.hpp
>$(CXX) $(CXXFLAGS) $(CURL_CFLAGS) $(INCLUDES) -c auth/auth.cpp -o obj/auth.o

obj/streamer.o: init_bin streamer/streamer.cpp streamer/streamer.h $(GENERATED_OBJS)
>$(CXX) $(CXXFLAGS) $(INCLUDES) -c streamer/streamer.cpp -o obj/streamer.o

obj/ecg_producer.o: init_bin ecg_producer.cpp ecg_producer.hpp auth/auth.hpp streamer/streamer.h
>$(CXX) $(CXXFLAGS) $(INCLUDES) -c ecg_producer.cpp -o obj/ecg_producer.o

obj/static/%.o: generated/%.cc init_bin libs
>$(CXX) $(STATIC_CXXFLAGS) $(INCLUDES) -c $< -o $@

obj/static/auth.o: init_bin libs auth/auth.cpp auth/auth.hpp
>$(CXX) $(STATIC_CXXFLAGS) $(CURL_CFLAGS) $(INCLUDES) -c auth/auth.cpp -o obj/static/auth.o

obj/static/streamer.o: init_bin libs streamer/streamer.cpp streamer/streamer.h obj/static/ecg.grpc.pb.o obj/static/ecg.pb.o obj/static/auth.o
>$(CXX) $(STATIC_CXXFLAGS) $(INCLUDES) -c streamer/streamer.cpp -o obj/static/streamer.o

obj/static/ecg_producer.o: init_bin libs ecg_producer.cpp ecg_producer.hpp auth/auth.hpp streamer/streamer.h
>$(CXX) $(STATIC_CXXFLAGS) $(INCLUDES) -c ecg_producer.cpp -o obj/static/ecg_producer.o

$(STATICLIB): $(STATIC_OBJS)
>ar rcs $(STATICLIB) $(STATIC_OBJS)

$(TARGET): main.cpp $(APP_OBJS) $(GENERATED_OBJS)
>$(CXX) $(CXXFLAGS) $(CURL_CFLAGS) $(INCLUDES) main.cpp $(APP_OBJS) $(GENERATED_OBJS) -o $(TARGET) $(LIBS_GRPC) $(CURL_LIBS)

clean:
>rm -f $(TARGET) $(STATICLIB) *.o
>rm -rf obj/static

init_bin:
>@mkdir -p bin
>@mkdir -p obj
>@mkdir -p obj/static

libs:
>@mkdir -p libs