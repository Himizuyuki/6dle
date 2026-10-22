CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -pedantic -O2 -g
CPPFLAGS = -Isrc -D_POSIX_C_SOURCE=200809L -MMD -MP
LDLIBS = -lm

BUILD = build
WORDS = data/wordBank.txt
MODEL = models/solver.nn

# Front ends find their data from any working directory.
PATHS = -DSIXDLE_WORDS='"$(CURDIR)/$(WORDS)"' \
        -DSIXDLE_MODEL='"$(CURDIR)/$(MODEL)"' \
        -DSIXDLE_ROOT='"$(CURDIR)"'

GTK_CFLAGS = $(shell pkg-config --cflags gtk+-3.0)
GTK_LIBS = $(shell pkg-config --libs gtk+-3.0)

LIB_SRC = $(wildcard src/core/*.c src/solver/*.c)
GUI_SRC = $(wildcard src/gui/*.c)
TERM_SRC = $(wildcard src/term/*.c)
TRAIN_SRC = tools/train.c
TEST_SRC = $(wildcard tests/*.c)

obj = $(patsubst %.c,$(BUILD)/obj/%.o,$(1))
LIB_OBJ = $(call obj,$(LIB_SRC))
GUI_OBJ = $(call obj,$(GUI_SRC))
TERM_OBJ = $(call obj,$(TERM_SRC))
TRAIN_OBJ = $(call obj,$(TRAIN_SRC))
# Tests are built separately with AddressSanitizer + UBSan.
TEST_OBJ = $(patsubst %.c,$(BUILD)/test-obj/%.o,$(LIB_SRC) $(TEST_SRC))
SANITIZE = -fsanitize=address,undefined -fno-omit-frame-pointer \
           -fno-sanitize-recover=all

GUI = $(BUILD)/6dle
TERM = $(BUILD)/6dle-term
TRAINER = $(BUILD)/6dle-train
TESTS = $(BUILD)/run-tests

.PHONY: all gui term trainer test check train eval run run-term format clean

all: gui term trainer

gui: $(GUI)
term: $(TERM)
trainer: $(TRAINER)

$(GUI): $(GUI_OBJ) $(LIB_OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(GTK_LIBS) $(LDLIBS)

$(TERM): $(TERM_OBJ) $(LIB_OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

$(TRAINER): $(TRAIN_OBJ) $(LIB_OBJ)
	$(CC) $(CFLAGS) -pthread -o $@ $^ $(LDLIBS)

$(BUILD)/obj/src/gui/%.o: src/gui/%.c
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(PATHS) $(CFLAGS) $(GTK_CFLAGS) -c -o $@ $<

$(BUILD)/obj/tools/%.o: tools/%.c
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) -pthread -c -o $@ $<

$(BUILD)/obj/%.o: %.c
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(PATHS) $(CFLAGS) -c -o $@ $<

# ---- tests --------------------------------------------------------------

# Unit tests (sanitized), then an end-to-end run of the real tools: train a
# tiny network on the fixture word list and let the terminal solver use it.
test: $(TESTS) $(TRAINER) $(TERM)
	./$(TESTS)
	@echo "end-to-end: train on tests/data/small.txt, then solve with it"
	./$(TRAINER) --words tests/data/small.txt --out $(BUILD)/smoke.nn \
		--games 300 --epochs 2 --dagger 1 --dagger-games 100 \
		--dagger-epochs 1 --eval 200 --threads 2 > $(BUILD)/smoke.log
	./$(TERM) --words tests/data/small.txt --model $(BUILD)/smoke.nn \
		--solve bottle > /dev/null
	@echo "all tests passed"

check: test

$(TESTS): $(TEST_OBJ)
	$(CC) $(CFLAGS) $(SANITIZE) -o $@ $^ $(LDLIBS)

$(BUILD)/test-obj/%.o: %.c
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) -Itests $(PATHS) $(CFLAGS) $(SANITIZE) -c -o $@ $<

# ---- solver model -------------------------------------------------------

# Retrain the network (a few minutes on all cores). TRAIN_ARGS lets you
# tweak it, e.g. make train TRAIN_ARGS="--games 40000 --epochs 12".
train: $(TRAINER)
	./$(TRAINER) --words $(WORDS) --out $(MODEL) $(TRAIN_ARGS)

# Measure the shipped model (and the entropy expert it imitates).
eval: $(TRAINER)
	./$(TRAINER) --words $(WORDS) --eval-only $(MODEL) --expert $(TRAIN_ARGS)

# ---- convenience --------------------------------------------------------

run: $(GUI)
	./$(GUI)

run-term: $(TERM)
	./$(TERM)

format:
	clang-format -i $(LIB_SRC) $(GUI_SRC) $(TERM_SRC) $(TRAIN_SRC) \
		$(TEST_SRC) $(wildcard src/*/*.h tests/*.h)

clean:
	rm -rf $(BUILD)

-include $(shell find $(BUILD) -name '*.d' 2>/dev/null)
