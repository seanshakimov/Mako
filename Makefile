# ╔═══════════════════════════════════════════════════╗
# ║          Mako Compiler - Build System             ║
# ╚═══════════════════════════════════════════════════╝

CC      = cc
CFLAGS  = -Wall -Wextra -O2 -std=c11
SRC_DIR = src
BUILD   = build
TARGET  = mako

SRCS = $(SRC_DIR)/main.c $(SRC_DIR)/lexer.c $(SRC_DIR)/parser.c $(SRC_DIR)/codegen.c
OBJS = $(BUILD)/main.o $(BUILD)/lexer.o $(BUILD)/parser.o $(BUILD)/codegen.o

.PHONY: all clean test playground

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^
	@echo ""
	@echo "  ✅ Mako compiler built successfully!"
	@echo "  Run: ./mako examples/hello.mako"
	@echo "  Or launch the web playground: make playground"
	@echo ""

$(BUILD)/%.o: $(SRC_DIR)/%.c | $(BUILD)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD):
	mkdir -p $(BUILD)

clean:
	rm -rf $(BUILD) $(TARGET)

playground: $(TARGET)
	@echo "🦈 Starting Mako Playground at http://localhost:8080 ..."
	@python3 web/server.py

test: $(TARGET)
	@echo "═══ Running Mako test suite ═══"
	@for f in examples/*.mako; do \
		echo ""; \
		echo "── Testing: $$f ──"; \
		./mako "$$f"; \
	done
