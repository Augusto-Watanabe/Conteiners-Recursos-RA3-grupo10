
# Verificação rigorosa de warnings
.PHONY: strict
strict: CFLAGS += -Wconversion -Wshadow -Wcast-qual -Wwrite-strings
strict: clean all
	@echo "Strict compilation complete!"

# Análise estática com cppcheck (se disponível)
.PHONY: analyze
analyze:
	@command -v cppcheck >/dev/null 2>&1 || { echo "cppcheck not installed"; exit 1; }
	cppcheck --enable=all --suppress=missingIncludeSystem $(SRC_DIR)/*.c

