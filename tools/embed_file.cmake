# V0.53: writes OUT, the files IN (a list, from src/) as one C string named NAME.
# The compiler copies it into the C of the programs that need it.
set(TEXT "")
foreach(PART ${IN})
    file(READ "${SRC}/src/${PART}" BODY)
    string(APPEND TEXT "/* ---- ${PART} ---- */\n${BODY}\n")
endforeach()
file(WRITE "${OUT}.txt" "${TEXT}")
file(READ "${OUT}.txt" HEX HEX)
string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," HEX "${HEX}")
string(REGEX REPLACE "(0x..,0x..,0x..,0x..,0x..,0x..,0x..,0x..,0x..,0x..,0x..,0x..,0x..,0x..,0x..,0x..,)" "\\1\n" HEX "${HEX}")
file(WRITE "${OUT}.tmp" "/* Generated from src/ by tools/embed_file.cmake */\nstatic const unsigned char ${NAME}[] = {\n${HEX}0x00\n};\n")
file(RENAME "${OUT}.tmp" "${OUT}")
