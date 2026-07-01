# Software Name: ib-ORAN
# SPDX-FileCopyrightText: Copyright (c) Orange SA
# SPDX-License-Identifier: MIT
#
# This software is distributed under the MIT license,
# see the "LICENSE" file for more details
# or https://spdx.org/licenses/MIT.html
#
# Authors: see CONTRIBUTORS.md

#!/bin/bash

set -e  # exit on error

echo ""
echo "ib-ORAN: Running setup and testing..."

# colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # no Color

CHECK_FAILED=0

# == CHECK PREREQUISITES ====================================================
echo -e "\n${YELLOW}[1/6] Checking prerequisites...${NC}"

# check if we're in ib-ORAN directory
IBORAN_EXP_DIR="contrib/iboran"
if [ -f "CMakeLists.txt" ]; then
    IBORAN_DIR="."
else
    IBORAN_DIR="$IBORAN_EXP_DIR"
fi
IBORAN_RUN_DIR="$IBORAN_DIR/model/iboran_campaign"
IBORAN_RUN_DIR_ABS="$(realpath $IBORAN_RUN_DIR)"
# export PYTHONPATH="$(realpath $IBORAN_DIR):$PYTHONPATH"

if [ ! -d "$IBORAN_DIR" ]; then
    echo -e "${RED}Error: directory not found. Run from ib-ORAN root directory.${NC}"
    exit 1
fi
echo -e "${GREEN}✓ ib-ORAN directory found${NC}"


# check Python 3.11+
if ! command -v python3 &> /dev/null; then
    echo -e "${RED}✗ Python 3 not found${NC}"
    CHECK_FAILED=1
else
    PYTHON_VERSION=$(python3 -c 'import sys; print(".".join(map(str, sys.version_info[:2])))')
    PYTHON_MAJOR=$(echo $PYTHON_VERSION | cut -d. -f1)
    PYTHON_MINOR=$(echo $PYTHON_VERSION | cut -d. -f2)
    
    if [ "$PYTHON_MAJOR" -lt 3 ] || ([ "$PYTHON_MAJOR" -eq 3 ] && [ "$PYTHON_MINOR" -lt 11 ]); then
        echo -e "${RED}✗ Python 3.11 required, found: $PYTHON_VERSION${NC}"
        CHECK_FAILED=1
    else
        echo -e "${GREEN}✓ Python $PYTHON_VERSION found${NC}"
    fi
fi


# check CMake 3.10+
if ! command -v cmake &> /dev/null; then
    echo -e "${RED}✗ CMake not found${NC}"
    CHECK_FAILED=1
else
    CMAKE_VERSION=$(cmake --version | head -n1 | grep -oE '[0-9]+\.[0-9]+\.[0-9]+')
    CMAKE_MAJOR=$(echo $CMAKE_VERSION | cut -d. -f1)
    CMAKE_MINOR=$(echo $CMAKE_VERSION | cut -d. -f2)
    
    if [ "$CMAKE_MAJOR" -lt 3 ] || ([ "$CMAKE_MAJOR" -eq 3 ] && [ "$CMAKE_MINOR" -lt 10 ]); then
        echo -e "${RED}✗ CMake 3.10+ required, found: $CMAKE_VERSION${NC}"
        CHECK_FAILED=1
    else
        echo -e "${GREEN}✓ CMake $CMAKE_VERSION found${NC}"
    fi
fi


# check if ns-3 is installed
if [ ! -d "../.." ]; then
    echo -e "${RED}✗ ns-3 root directory not found${NC}"
    CHECK_FAILED=1
else
    echo -e "${GREEN}✓ ns-3 directory found${NC}"
    
    # check for ns3 build script
    if [ -f "../../ns3" ]; then
        echo -e "${GREEN}✓ ns3 build script found${NC}"
    else
        echo -e "${RED}✗ ns3 build script not found${NC}"
        CHECK_FAILED=1
    fi
fi

# check if ns3-ai is installed
if [ ! -d "../ai" ]; then
    echo -e "${RED}✗ ns3-ai module not found in contrib/ai${NC}"
    CHECK_FAILED=1
else
    echo -e "${GREEN}✓ ns3-ai module found${NC}"
fi

# check if nr ns-3 is installed
if [ ! -d "../nr" ]; then
    echo -e "${RED}✗ NR ns-3 not found in contrib/nr${NC}"
    CHECK_FAILED=1
else
    echo -e "${GREEN}✓ NR ns-3 module found${NC}"
fi


# check boost 1.89+
BREW_PREFIX=""
if [[ "$OSTYPE" == "darwin"* ]] && command -v brew &>/dev/null; then
    BREW_PREFIX=$(brew --prefix)
fi

BOOST_HEADER_PATHS=(
    "/usr/include"
    "/usr/local/include"
    "$BREW_PREFIX/include"
)

BOOST_HEADERS_ROOT=""
for p in "${BOOST_HEADER_PATHS[@]}"; do
    if [ -d "$p/boost" ]; then
        BOOST_HEADERS_ROOT="$p"
        break
    fi
done


if [[ -n "$BOOST_HEADERS_ROOT" ]]; then
    
    VERSION_HEADER="$BOOST_HEADERS_ROOT/boost/version.hpp"

    if [ -f "$VERSION_HEADER" ]; then
        
        BOOST_VERSION_DECIMAL=$(grep '#define BOOST_VERSION ' "$VERSION_HEADER" | awk '{print $NF}')
        
        if [[ -z "$BOOST_VERSION_DECIMAL" ]]; then
            echo -e "${RED}✗ Boost version file found but failed to extract version.${NC}"
            CHECK_FAILED=1
        else
            # convert the decimal version (e.g., 108900) to a displayable X.Y.Z format
            # major is (108900 / 100000) = 1
            MAJOR=$(( $BOOST_VERSION_DECIMAL / 100000 ))
            # minor is ((108900 / 100) % 1000) = 89
            MINOR=$(( ($BOOST_VERSION_DECIMAL / 100) % 1000 ))
            # patch is (108900 % 100) = 0
            PATCH=$(( $BOOST_VERSION_DECIMAL % 100 ))
            
            BOOST_VERSION="$MAJOR.$MINOR.$PATCH"
            
            # the required version 1.89.0 converts to the decimal 108900
            # target check: 1 * 100000 + 89 * 100 + 0 = 108900
            REQUIRED_VERSION_DECIMAL=108900
            
            # numerical comparison
            if [ "$BOOST_VERSION_DECIMAL" -ge "$REQUIRED_VERSION_DECIMAL" ]; then
                echo -e "${GREEN}✓ Boost $BOOST_VERSION found${NC}"
            else
                echo -e "${RED}✗ Boost 1.89+ required, found: $BOOST_VERSION${NC}"
                CHECK_FAILED=1
            fi
        fi

    else
        echo -e "${RED}✗ Boost headers found but 'boost/version.hpp' is missing.${NC}"
        CHECK_FAILED=1
    fi

else
    echo -e "${RED}✗ Boost not found${NC}"
    echo -e "${YELLOW} Install: sudo apt install libboost-all-dev (Ubuntu/Debian)${NC}"
    echo -e "${YELLOW}         brew install boost (macOS)${NC}"
    CHECK_FAILED=1
fi


# check Protocol Buffers 6.33.0+
if ! command -v protoc &> /dev/null; then
    echo -e "${RED}✗ Protocol Buffers (protoc) not found${NC}"
    echo -e "${YELLOW}  Install: sudo apt install protobuf-compiler libprotobuf-dev (Ubuntu/Debian)${NC}"
    echo -e "${YELLOW}          brew install protobuf (macOS)${NC}"
    CHECK_FAILED=1
else
    PROTOC_VERSION=$(protoc --version | grep -oE '[0-9]+\.[0-9]+')
    PROTOC_MAJOR=$(echo $PROTOC_VERSION | cut -d. -f1)
    PROTOC_MINOR=$(echo $PROTOC_VERSION | cut -d. -f2)
    
    if [ "$PROTOC_MAJOR" -lt 6 ] || ([ "$PROTOC_MAJOR" -eq 6 ] && [ "$PROTOC_MINOR" -lt 33 ]); then
        echo -e "${RED}✗ Protocol Buffers 6.33.0+ required, found: $PROTOC_VERSION${NC}"
        CHECK_FAILED=1
    else
        echo -e "${GREEN}✓ Protocol Buffers $PROTOC_VERSION found${NC}"
    fi 
fi


# check for C++ compiler
if command -v g++ &> /dev/null; then
    GCC_VERSION=$(g++ --version | head -n1 | grep -oE '[0-9]+\.[0-9]+\.[0-9]+|[0-9]+\.[0-9]+' | head -n1)
    GCC_MAJOR=$(echo $GCC_VERSION | cut -d. -f1)
    GCC_MINOR=$(echo $GCC_VERSION | cut -d. -f2)

    GCC_MAJOR=${GCC_MAJOR:-0}
    GCC_MINOR=${GCC_MINOR:-0}
    
    if [ "$GCC_MAJOR" -lt 11 ] || ([ "$GCC_MAJOR" -eq 11 ] && [ "$GCC_MINOR" -lt 1 ]); then
        echo -e "${RED}✗ g++ 11.1+ required, found: $GCC_VERSION${NC}"
        CHECK_FAILED=1
    else
        echo -e "${GREEN}✓ g++ found: $GCC_VERSION${NC}"
    fi
elif command -v clang++ &> /dev/null; then
    CLANG_FULL_OUTPUT=$(clang++ --version | head -n1)
    CLANG_VERSION=$(echo "$CLANG_FULL_OUTPUT" | grep -oE '([0-9]+\.[0-9]+(\.[0-9]+)?)' | head -n1)
    CLANG_MAJOR=$(echo $CLANG_VERSION | cut -d. -f1)
    CLANG_MINOR=$(echo $CLANG_VERSION | cut -d. -f2)

    CLANG_MAJOR=${CLANG_MAJOR:-0}
    CLANG_MINOR=${CLANG_MINOR:-0}
    
    if [ "$CLANG_MAJOR" -lt 17 ] || ([ "$CLANG_MAJOR" -eq 17 ] && [ "$CLANG_MINOR" -lt 0 ]); then
        echo -e "${RED}✗ clang++ 17.0+ required, found: $CLANG_VERSION${NC}"
        CHECK_FAILED=1
    else
        echo -e "${GREEN}✓ clang++ found: $CLANG_VERSION${NC}"
    fi
else
    echo -e "${RED}✗ C++ compiler not found${NC}"
    CHECK_FAILED=1
fi

# Exit if any prerequisite check failed
if [ $CHECK_FAILED -eq 1 ]; then
    echo -e "${RED}Prerequisites check FAILED. Please install missing dependencies.${NC}"
    exit 1
fi


# == CHECK SOURCE FILES =====================================================
echo -e "\n${YELLOW}[2/6] Checking source files...${NC}"

cd "$IBORAN_DIR"

REQUIRED_FILES=(
    "model/iboran_application/iboran-application.h"
    "model/iboran_application/iboran-application.cc"
    "model/iboran_application/iboran-side-info-tracker.h"
    "model/iboran_application/iboran-side-info-tracker.cc"
    "model/iboran_interfaces/iboran-shm-structure.h"
    "model/iboran_interfaces/iboran-shm-structure.cc"
    "model/iboran_interfaces/iboran_shm_structure.py"
    "model/iboran_interfaces/iboran_shm_structure_py_binding.cc"
    "model/iboran_ran_environment/iboran-lte-environment.cc"
    "model/iboran_ran_environment/iboran-nr-environment.cc"
    "model/iboran_ran_environment/iboran-ran-params.h"
    "model/iboran_ran_environment/iboran-ran-params.cc"
    "model/iboran_ran_environment/iboran-ran-common.h"
    "model/iboran_ran_environment/iboran-ran-common.cc"
    "model/iboran_mac_scheduler/iboran-lte-mac-scheduler.h"
    "model/iboran_mac_scheduler/iboran-lte-mac-scheduler.cc"
    "model/iboran_mac_scheduler/iboran-nr-mac-scheduler.h"
    "model/iboran_mac_scheduler/iboran-nr-mac-scheduler.cc"
    "model/iboran_campaign/iboran_orchestrator.py"
    "model/iboran_campaign/iboran_run.py"
    "model/iboran_campaign/iboran_campaign.py"
    "model/iboran_controller/iboran_dapp.py"
    "model/iboran_controller/iboran_xapp.py"
    "model/iboran_controller/iboran_dapp_algos/dapp_algos_factory.py"
    "model/iboran_controller/iboran_xapp_algos/xapp_algos_factory.py"
    "CMakeLists.txt"
)

MISSING_FILES=0
for file in "${REQUIRED_FILES[@]}"; do
    if [ ! -f "$file" ]; then
        echo -e "${RED}✗ Missing: $file${NC}"
        MISSING_FILES=$((MISSING_FILES + 1))
    else
        echo -e "${GREEN}✓ Found: $file${NC}"
    fi
done

if [ $MISSING_FILES -gt 0 ]; then
    echo -e "\n${RED}Error: $MISSING_FILES file(s) missing.${NC}"
    echo "Please copy all files to $IBORAN_DIR/"
    exit 1
fi


# == CHECK/CREATE VIRTUAL ENVIRONMENT =======================================
echo -e "${YELLOW}[3/6] Setting up Python virtual environment...${NC}"

VENV_DIR="../../.venv"

if [ -d "$VENV_DIR" ]; then
    echo -e "${GREEN}✓ Virtual environment already exists${NC}"
else
    echo "Creating virtual environment..."
    python3 -m venv "$VENV_DIR"
    echo -e "${GREEN}✓ Virtual environment created${NC}"
fi

# Activate virtual environment
source "$VENV_DIR/bin/activate"
echo -e "${GREEN}✓ Virtual environment activated${NC}"


# == INSTALL PYTHON DEPENDENCIES ============================================
echo -e "\n${YELLOW}[4/6] Installing Python dependencies...${NC}"

# upgrade pip
pip install --upgrade pip > /dev/null 2>&1

# install from requirements.txt if it exists
if [ -f "requirements.txt" ]; then
    echo "Installing from requirements.txt..."
    pip install -r requirements.txt
    echo -e "${GREEN}✓ Requirements installed${NC}"
fi

# check/install ns3ai_utils
python3 -c "import ns3ai_utils" 2>/dev/null && {
    echo -e "${GREEN}✓ ns3ai_utils already installed${NC}"
} || {
    echo "Installing ns3ai_utils..."
    if [ -d "../../python_utils" ]; then
        pip install -e ../../python_utils/
        echo -e "${GREEN}✓ ns3ai_utils installed${NC}"
    else
        echo -e "${RED}✗ Warning: contrib/ai/python_utils not found${NC}"
        CHECK_FAILED=1
    fi
}

# # check pybind11 3.0+
# python3 -c "import pybind11" 2>/dev/null && {
#     PYBIND_VERSION=$(python3 -c "import pybind11; print(pybind11.__version__)")
#     PYBIND_MAJOR=$(echo $PYBIND_VERSION | cut -d. -f1)
#     PYBIND_MINOR=$(echo $PYBIND_VERSION | cut -d. -f2)
    
#     if [ "$PYBIND_MAJOR" -lt 3 ] || ([ "$PYBIND_MAJOR" -eq 3 ] && [ "$PYBIND_MINOR" -lt 0 ]); then
#         echo -e "${RED}✗ pybind11 3.0+ required, found: $PYBIND_VERSION${NC}"
#         CHECK_FAILED=1
#     else
#         echo -e "${GREEN}✓ pybind11 $PYBIND_VERSION found${NC}"
#     fi
# } || {
#     echo -e "${RED}✗ pybind11 not found${NC}"
#     echo -e "${YELLOW}  Install: pip3 install pybind11${NC}"
#     CHECK_FAILED=1
# }

# check/install pybind11 in venv
python3 -c "import pybind11" 2>/dev/null && {
    echo -e "${GREEN}✓ pybind11 available in virtual environment${NC}"
} || {
    echo "Installing pybind11 in virtual environment..."
    pip install pybind11
    echo -e "${GREEN}✓ pybind11 installed${NC}"
}

# # install ib-ORAN package itself
# echo "Installing ib-ORAN package..."
# pip install -e .
# echo -e "${GREEN}✓ ib-ORAN package installed${NC}"

# register iboran path in venv
SITE_PACKAGES=$(python3 -c "import site; print(site.getsitepackages()[0])")
IBORAN_ABS_PATH="$(realpath $IBORAN_DIR)"   # capture while path is still valid
echo "$IBORAN_ABS_PATH" > "$SITE_PACKAGES/iboran.pth"
echo -e "${GREEN}✓ ib-ORAN path registered: $IBORAN_ABS_PATH${NC}"


# == BUILD C++ EXECUTABLE ===================================================
echo -e "\n${YELLOW}[5/6] Building C++ executable...${NC}"

cd ../..  # go to ns-3 root directory
echo "Configuring ns-3..."
./ns3 configure --enable-examples --enable-tests

echo "Building iboran..."
./ns3 build iboran

if [ $? -eq 0 ]; then
    echo -e "${GREEN}✓ Build successful${NC}"
else
    echo -e "${RED}✗ Build failed${NC}"
    exit 1
fi


# == FINALIZE ===============================================================
IBORAN_DIR="$IBORAN_EXP_DIR"

echo -e "\n${YELLOW}[6/6] Finalizing setup...${NC}"

# make Python script executable
cd "$IBORAN_RUN_DIR_ABS"
chmod +x "iboran_run.py"

echo -e "${GREEN}✓ Setup complete${NC}"

echo ""
echo "========================================="
echo ""
echo "To quickly run the simulation:"
echo ""
echo "  $ cd $IBORAN_RUN_DIR"
echo "  $ python3 iboran_run.py"
echo ""
echo "========================================="


# == INTERACTIVE TEST (OPTIONAL) ============================================
echo ""
read -p "Would you like to run a quick test now? (y/n) " -n 1 -r
echo
if [[ $REPLY =~ ^[Yy]$ ]]; then
    echo -e "\n${YELLOW}Starting test...${NC}"
    
    # start Python agent in background
    python3 iboran_run.py --rat_type "NR" --dapp_algo_ue_selection "Random" --k 5 --save_dapp_log false --dapp_control_period_sf 8
    
    echo -e "\n${GREEN}✓ Test completed!${NC}"
    echo "If you saw scheduling decisions being made, everything is working!"
fi

echo ""
echo -e "${GREEN}All done!${NC}"