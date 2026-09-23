# Tests for the driver, run against a software model of the receiver (tests/fake).
# No OpenRGB build, Qt or hardware needed. See tests/README.md.
#
#   make test               all three suites
#   make test-unit          transport and protocol encoding
#   make test-integration   detector + controllers + modes against the fake firmware
#   make test-e2e           whole flow, plus the shell tools and the udev rule
#   make test JUNIT=1       also write build/tests/*.xml reports
CXX      ?= g++
CXXFLAGS ?= -std=c++17 -Wall -Wextra -Wno-pragmas -g -O0
INCLUDES  = -Itests/stubs -Itests/fake -Isrc/DareuEK75Controller -Itests/vendor/cest
OUT       = build/tests

DRIVER    = src/DareuEK75Controller/DareuEK75Controller.cpp \
            src/DareuEK75Controller/RGBController_DareuEK75.cpp \
            src/DareuEK75Controller/DareuEK75ControllerDetect.cpp
FAKE      = tests/fake/fake_receiver.cpp

SUITES    = unit integration e2e

.PHONY: test $(addprefix test-,$(SUITES)) test-color-transform clean

test: $(addprefix test-,$(SUITES)) test-color-transform

# Pure-function tests for the theme colour transform, no OpenRGB or keyboard involved.
# Kept as plain unittest scripts (stdlib only) rather than a fourth Cest suite, since
# they test tools/color_transform.py and tools/calibrate_color.py directly in Python.
test-color-transform:
	python3 tests/unit/test_color_transform.py -v
	python3 tests/unit/test_calibrate_color.py -v

define suite
$(OUT)/$(1): $(wildcard tests/$(1)/*.cpp) $(DRIVER) $(FAKE) $(wildcard tests/fake/*.h tests/stubs/*.h tests/common/*.h src/DareuEK75Controller/*.h)
	@mkdir -p $(OUT)
	$$(CXX) $$(CXXFLAGS) $$(INCLUDES) -Itests/common -o $$@ $(wildcard tests/$(1)/*.cpp) $(DRIVER) $(FAKE)

test-$(1): $(OUT)/$(1)
	$(OUT)/$(1) $(if $(JUNIT),--junit $(OUT)/$(1).xml)
endef

$(foreach s,$(SUITES),$(eval $(call suite,$(s))))

clean:
	rm -rf $(OUT)
