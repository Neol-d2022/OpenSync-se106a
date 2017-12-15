echo "OBJS=$(ls *.c | sed 's/\.c/\.o/g' | tr '\n' ' ')"
echo "DEPS=$(ls *.h | tr '\n' ' ')"
