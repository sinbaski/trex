function l = fnum_of_line(fd)

DATA_ROW_WIDTH = 32;
x = ftell(fd);
fseek(fd, 0, 'eof');
l = ftell(fd) / DATA_ROW_WIDTH;
fseek(fd, x, 'bof');

