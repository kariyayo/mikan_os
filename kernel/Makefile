# 生成するカーネルのバイナリ名
TARGET = kernel.elf

# リンク対象のオブジェクトファイル一覧
OBJS = main.o graphics.o font.o hankaku.o newlib_support.o console.o pci.o asmfunc.o

# ヘッダファイルの依存関係ファイル一覧 (.main.d, .graphics.d, .font.d)
# カレントディレクトリが散らからないよう、先頭にドット '.' を付与した隠しファイル名を動的に生成
# 1. $(OBJS:.o=.d)                  : .o を .d に置換 (main.d, graphics.d, font.d)
# 2. $(notdir ...)                  : ファイル名部分のみを抽出
# 3. $(addprefix ., ...)            : 先頭に '.' を付与 (.main.d, .graphics.d, .font.d)
# 4. $(join $(dir $(OBJS)), ...)    : ディレクトリパスとファイル名を結合
DEPENDS = $(join $(dir $(OBJS)),$(addprefix .,$(notdir $(OBJS:.o=.d))))

CFLAGS   += -O2 -Wall -g --target=x86_64-elf -ffreestanding -mno-red-zone
CXXFLAGS += -O2 -Wall -g --target=x86_64-elf -ffreestanding -mno-red-zone \
			-fno-exceptions -fno-rtti -std=c++17
LDFLAGS  += --entry KernelMain -z norelro --image-base 0x100000 --static -z separate-code


.PHONY: all
all: $(TARGET)

.PHONY: clean
clean:
	rm -rf *.o

kernel.elf: $(OBJS) Makefile
	ld.lld $(LDFLAGS) -o kernel.elf $(OBJS) -lc

%.o: %.cpp Makefile
	clang++ $(CPPFLAGS) $(CXXFLAGS) -c $<

.%.d: %.cpp
	# -MM : ソースコード内の #include を解析し、Makeの依存関係形式で出力（標準ヘッダは除外）
	clang++ $(CPPFLAGS) $(CXXFLAGS) -MM $< > $@
	# サブディレクトリがある場合、依存関係のターゲットパスを正しく補正
	$(eval OBJ = $(<:.cpp=.o))
	sed --in-place 's|$(notdir $(OBJ))|$(OBJ)|' $@

%.o: %.c Makefile
	clang $(CPPFLAGS) $(CFLAGS) -c $<

.%.d: %.c
	clang $(CPPFLAGS) $(CFLAGS) -MM $< > $@
	$(eval OBJ = $(<:.c=.o))
	sed --in-place 's|$(notdir $(OBJ))|$(OBJ)|' $@

%.o: %.asm Makefile
	nasm -f elf64 -o $@ $<

hankaku.bin: hankaku.txt
	../tools/makefont.py -o $@ $<

hankaku.o: hankaku.bin
	objcopy -I binary -O elf64-x86-64 -B i386:x86-64 $< $@

.%.d: %.bin
	touch $@

.PHONY: depends
depends:
	$(MAKE) $(DEPENDS)

# 生成された依存関係ファイル (.main.d など) を Makefile の一部として読み込む
# 先頭のハイフン '-' により、初回ビルド時などファイルがまだ存在しなくてもエラーを無視する
-include $(DEPENDS)
