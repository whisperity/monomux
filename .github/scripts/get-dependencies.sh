#!/bin/bash
set -ex

echo "::group::Updating package lists"
# Remove this so we get to test with the compiler that is **OFFICIALLY**
# available.
# sudo apt-get install -y software-properties-common # Supplies add-apt-repository.
# sudo add-apt-repository --remove -y "ppa:ubuntu-toolchain-r/test"
# sudo apt-get -y clean
# sudo rm -rf /var/lib/apt/lists/*
sudo apt-get -y update
echo "::endgroup::"

echo "::group::Installing compiler $COMPILER"
# Get the latest version of the compiler available for the distribution.
# CC=$(apt-cache search $(echo "$COMPILER") \
#     | grep "^${COMPILER}-" \
#     | cut -d "-" -f 1-2 \
#     | grep "\-[0-9]" \
#     | sort -rV \
#     | head -n 1 \
#     )
#
# Unfortunately, GitHub Actions is a bit ridiculous, and they seem to not let
# you *PROPERLY* clear the package list and fetch the package that is available
# as the latest version because they use a lot of custom package sources...
#
# So I guess we're just hardcoding these for now...
if [[ "${COMPILER}" == "gcc" ]]
then
    if [[ "${DISTRO}" == "ubuntu-18.04" ]]
    then
        CC="gcc-8"
    elif [[ "${DISTRO}" == "ubuntu-20.04" ]]
    then
        CC="gcc-10"
    fi

    CXX=$(echo ${CC} | sed 's/gcc/g++/')
    # With GCC, you need to install G++-X to get C++ compilation.
    sudo apt-get -y install ${CXX}
elif [[ "${COMPILER}" == "clang" ]];
then
    if [[ "${DISTRO}" == "ubuntu-18.04" ]]
    then
        CC="clang-10"
    elif [[ "${DISTRO}" == "ubuntu-20.04" ]]
    then
        CC="clang-12"
    fi

    CXX=$(echo ${CC} | sed 's/clang/clang++/')
    # Clang works both ways.
    sudo apt-get -y install ${CC}
fi
echo "Using compiler ${CC} (${CXX})..."
echo "::set-output name=CC::${CC}"
echo "::set-output name=CXX::${CXX}"
echo "::endgroup::"

echo "::group::Installing CMake"
# These Ubuntu images of GitHub Actions are getting really annoying... so turns
# out that they completely disrespect their users by having unwanted stuff
# installed under /usr/**local** which gets priority over what the user would
# like to **EXPLICITLY** install via the package manager...
sudo rm -v $(which cmake) $(which cpack)

sudo apt-get install -y \
    lsb-release \
    wget
wget -O - https://apt.kitware.com/keys/kitware-archive-latest.asc \
    2>/dev/null \
    | gpg --dearmor - \
    | sudo tee /etc/apt/trusted.gpg.d/kitware.gpg >/dev/null
sudo apt-add-repository -y \
    "deb https://apt.kitware.com/ubuntu/ $(lsb_release -cs) main"
sudo apt-get -y update

if [[ "${DISTRO}" == "ubuntu-18.04" ]]
then
    # We need a newer version of CMake than supplied by Ubuntu...
    # FetchContent is only available 3.11 and onwards, Ubuntu 18 gives only
    # 3.10.
    sudo apt-get -y install \
        cmake="3.15.0-0kitware1" \
        cmake-data="3.15.0-0kitware1"
elif [[ "${DISTRO}" == "ubuntu-20.04" ]]
then
    # The lowest version available on the Kitware PPA for Ubuntu 20.04.
    sudo apt-get -y install \
        cmake="3.17.2-0kitware1ubuntu20.04.1" \
        cmake-data="3.17.2-0kitware1ubuntu20.04.1"
fi
echo "::endgroup::"
