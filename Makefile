CC := gcc
SRCD := src
BLDD := build
INCD := include
TFLD := testFiles
TLD  := tests

MAIN  := $(BLDD)/main.o

ALL_SRCF := $(shell find $(SRCD) -type f -name *.c)
ALL_OBJF := $(patsubst $(SRCD)/%,$(BLDD)/%,$(ALL_SRCF:.c=.o))
ALL_FUNCF := $(filter-out $(MAIN) $(AUX), $(ALL_OBJF))

INC := -I $(INCD)

CFLAGS := -g -O2 -Wall -Werror -Wno-unused-variable -Wno-unused-function -MMD
COLORF := -DCOLOR
DFLAGS := -g -DDEBUG -DCOLOR
PRINT_STAMENTS := -DERROR -DSUCCESS -DWARN -DINFO

STD := -std=gnu11
LIBS := -lm -lssl -lcrypto

CFLAGS += $(STD)

EXEC := tish

.PHONY: clean all setup debug tests wipe

all: setup $(EXEC)

debug: CFLAGS += $(DFLAGS) $(PRINT_STAMENTS) $(COLORF)
debug: all

setup: $(BLDD)

$(BLDD):
				mkdir -p $(BLDD)

$(EXEC): $(ALL_OBJF)
				$(CC) $^ -o $@ $(LIBS)

$(BLDD)/%.o: $(SRCD)/%.c
				$(CC) $(CFLAGS) $(INC) -c -o $@ $<

clean:
				rm -rf $(BLDD)
				rm -rf $(EXEC)

tests: clean all
				@if [ ! -d "$(TLD)/testOutput" ]; then mkdir $(TLD)/testOutput; fi
				@echo "\n"
				bash -c $(TLD)/test2.sh
				bash -c $(TLD)/test1.sh

wipe:
				truncate -s 0 $(TFLD)/middleFile.txt
				truncate -s 0 $(TFLD)/outFile.txt

.PRECIOUS: $(BLDD)/*.d
-include $(BLDD)/*.d