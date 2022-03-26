#!/bin/bash
set -x

echo "::group::Updating package lists"
sudo apt-get -y update
echo "::endgroup::"

# Get the latest version of the compiler available for the distribution.
echo "::group::Installing compiler $COMPILER"
CC=$(apt-cache search $(echo "$COMPILER") \
    | grep "^${COMPILER}-" \
    | cut -d "-" -f 1-2 \
    | grep "\-[0-9]" \
    | sort -rV \
    | head -n 1 \
    )
if [[ "${COMPILER}" == "gcc" ]];
then
    CXX=$(echo ${CC} | sed 's/gcc/g++/')
    # With GCC, you need to install G++-X to get C++ compilation.
    sudo apt-get -y install ${CXX}
elif [[ "${COMPILER}" == "clang" ]];
then
    CXX=$(echo ${CC} | sed 's/clang/clang++/')
    # Clang works both ways.
    sudo apt-get -y install ${CC}
fi
echo "Using compiler ${CC} (${CXX})..."
echo "::set-output name=CC::${CC}"
echo "::set-output name=CXX::${CXX}"
echo "::endgroup::"

echo "::group::Installing CMake"
if [[ "${DISTRO}" == "ubuntu-18.04" ]]
then
    # We need a newer version of CMake than supplied by Ubuntu...
    # FetchContent is only available 3.11 and onwards, Ubuntu gives
    # 3.10.
    sudo apt-get install -y software-properties-common lsb-release
    sudo apt-get install -y wget
    wget -O - https://apt.kitware.com/keys/kitware-archive-latest.asc \
        2>/dev/null \
        | gpg --dearmor - \
        | sudo tee /etc/apt/trusted.gpg.d/kitware.gpg >/dev/null
    sudo apt-add-repository -y \
        "deb https://apt.kitware.com/ubuntu/ $(lsb_release -cs) main"
    sudo apt-get -y update
    sudo apt-get -y install \
        cmake="3.15.0-0kitware1" \
        cmake-data="3.15.0-0kitware1"
elif [[ "${DISTRO}" == "ubuntu-20.04" ]]
then
    sudo apt-get -y install \
    cmake
fi
echo "::endgroup::"
