# Writes OUT: the big-number runtime (bigint, bigfloat, collector) as one C string.
# The compiler copies it into the C of every program that uses bigint or bigfloat:
# the program needs no other file, wherever clair is run from (V0.42).
set(PARTS clair_bigint.h clair_bigint.c clair_bigfloat.h clair_bigfloat.c
          clair_bigmath.h clair_bigmath.c
          clair_certfloat.h clair_certfloat.c
          clair_big_numbers_rt.h clair_big_numbers_rt.c clair_certfloat_rt.c)
set(TEXT "/* ---- Clair : grands nombres dynamiques (bigint, bigfloat) ---- */\n")
foreach(PART ${PARTS})
    file(READ "${SRC}/src/${PART}" BODY)
    string(REGEX REPLACE "#include \"clair_[a-z_]+\\.h\"" "/* ${PART} : en-tetes Clair deja inclus */" BODY "${BODY}")
    string(APPEND TEXT "/* ---- ${PART} ---- */\n${BODY}\n")
endforeach()
string(APPEND TEXT "/* ---- fin des grands nombres ---- */\n")
file(WRITE "${OUT}.txt" "${TEXT}")
file(READ "${OUT}.txt" HEX HEX)
string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," HEX "${HEX}")
string(REGEX REPLACE "(0x..,0x..,0x..,0x..,0x..,0x..,0x..,0x..,0x..,0x..,0x..,0x..,0x..,0x..,0x..,0x..,)" "\\1\n" HEX "${HEX}")
file(WRITE "${OUT}.tmp" "/* Generated from src/clair_big*.{h,c} by tools/embed_runtime.cmake */\nstatic const unsigned char clair_big_runtime_text[] = {\n${HEX}0x00\n};\n")
file(RENAME "${OUT}.tmp" "${OUT}")
