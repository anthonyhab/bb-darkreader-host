CC ?= cc
CFLAGS ?= -O2 -pipe -fstack-protector-strong -fstack-clash-protection -Wp,-D_FORTIFY_SOURCE=3 -fPIE
LDFLAGS ?= -pie -Wl,-z,relro,-z,now,-z,noexecstack
WARNINGS := -Wall -Wextra -Wpedantic
LDLIBS := -ljson-c
TARGET := bb-darkreader-host
SOURCE := bb-darkreader-host.c

.PHONY: all clean test sanitize analyze ci

all: $(TARGET)

$(TARGET): $(SOURCE) bb-common/native_messaging.h bb-common/json_utils.h bb-common/config_utils.h
	$(CC) $(CFLAGS) $(WARNINGS) -std=c11 -o $@ $(SOURCE) $(LDLIBS) $(LDFLAGS)

test: $(TARGET)
	HOST_BINARY="$(CURDIR)/$(TARGET)" python3 tests/test_protocol.py

ci:
	$(MAKE) clean
	$(MAKE) $(TARGET) WARNINGS="$(WARNINGS) -Werror"
	HOST_BINARY="$(CURDIR)/$(TARGET)" python3 tests/test_protocol.py

sanitize:
	$(CC) -O1 -g $(WARNINGS) -std=c11 -fsanitize=address,undefined -fno-omit-frame-pointer -o $(TARGET)-sanitize $(SOURCE) $(LDLIBS) $(LDFLAGS)
	HOST_BINARY="$(CURDIR)/$(TARGET)-sanitize" python3 tests/test_protocol.py
	rm -f $(TARGET)-sanitize

analyze:
	$(CC) $(CFLAGS) $(WARNINGS) -std=c11 -fanalyzer -fsyntax-only $(SOURCE)

clean:
	rm -f $(TARGET) $(TARGET)-sanitize
