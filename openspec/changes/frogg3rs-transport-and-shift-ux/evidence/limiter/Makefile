# Builds the Filter-limiter measurement binaries into WORK/bin. Run from the
# WORK directory setup.sh filled, after building Sheaf's library
# (make -C External/Sheaf/projects/synth build):
#
#   cd WORK && nice make -j2 -f <this directory>/Makefile [harness|tests|all]
#
# harness  bin/harness_app, bin/harness_cur, bin/harness_moved (modes: grid,
#          order, hashcheck), bin/combbound, bin/pt_cur, bin/pt_moved
# tests    each suite binary built from cur/ and from moved/, the
#          limiter-bypassed routing tests bin/rb_cur and bin/rb_moved, and
#          bin/rrepin and bin/rbreak from moved/
.SECONDEXPANSION:
EVIDENCE := $(patsubst %/,%,$(dir $(abspath $(lastword $(MAKEFILE_LIST)))))
REPO ?= $(abspath $(EVIDENCE)/../../../../..)
SH := $(REPO)/External/Sheaf/projects/synth
LIB := $(SH)/build/libsynth.a
CXX := clang++
FLAGS := -std=c++20 -Wall -Wextra -O2 -DFROGGERS_DSP_CHECKS
INC = -I$(SH)/include -I$(SH)/tests

HARNESS := bin/harness_app bin/harness_cur bin/harness_moved bin/combbound bin/pt_cur bin/pt_moved
TESTS := $(foreach v,cur moved,bin/dsp_$(v) bin/routing_$(v) bin/visualizer_$(v) bin/headless_$(v) bin/parammodel_$(v) bin/rb_$(v)) \
         bin/rrepin bin/rbreak

.PHONY: all harness tests
all: harness tests
harness: $(HARNESS)
tests: $(TESTS)

bin:
	mkdir -p bin

bin/harness_app: $(EVIDENCE)/harness.cpp | bin
	$(CXX) $(FLAGS) -Iapp $(INC) $< $(LIB) -o $@
bin/harness_%: $(EVIDENCE)/harness.cpp $$*/FroggersAppCore.hpp $$*/dsp/FilterFx.hpp | bin
	$(CXX) $(FLAGS) -DTAPS -I$* $(INC) $< $(LIB) -o $@
bin/combbound: $(EVIDENCE)/combbound.cpp | bin
	$(CXX) $(FLAGS) -Iapp -I$(SH)/include $< -o $@
bin/pt_%: $(EVIDENCE)/placementtest.cpp $$*/dsp/FilterFx.hpp | bin
	$(CXX) $(FLAGS) -I$* -I$(SH)/include $< -o $@

bin/dsp_%: $$*/FroggersDspParityTests.cpp $$*/dsp/FilterFx.hpp | bin
	$(CXX) $(FLAGS) -I$* -I$(SH)/include $< -o $@
bin/routing_%: $$*/FroggersAudioRoutingTests.cpp $$*/dsp/FilterFx.hpp | bin
	$(CXX) $(FLAGS) -I$* $(INC) $< $(LIB) -o $@
bin/visualizer_%: $$*/FroggersVisualizerTests.cpp $$*/dsp/FilterFx.hpp | bin
	$(CXX) $(FLAGS) -I$* $(INC) $< $(LIB) -o $@
bin/headless_%: $$*/FroggersHeadlessTests.cpp $$*/dsp/FilterFx.hpp | bin
	$(CXX) $(FLAGS) -I$* $(INC) $< $(LIB) -o $@
bin/parammodel_%: $$*/FroggersParameterModelTests.cpp $$*/dsp/FilterFx.hpp | bin
	$(CXX) $(FLAGS) -I$* $(INC) $< $(LIB) -o $@
bin/rb_%: $$*/FroggersAudioRoutingTestsBypass.cpp | bin
	$(CXX) $(FLAGS) -I$* $(INC) $< $(LIB) -o $@
bin/rrepin: moved/FroggersAudioRoutingTestsRepin.cpp | bin
	$(CXX) $(FLAGS) -Imoved $(INC) $< $(LIB) -o $@
bin/rbreak: moved/FroggersAudioRoutingTestsBreak.cpp | bin
	$(CXX) $(FLAGS) -Imoved $(INC) $< $(LIB) -o $@
