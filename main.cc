#include <string>
#include <iostream>

using namespace std;

string BrotliCompressTrivial(const string& u) {
	if (u.empty()) {
		return string(1, 6);
	}
	int i;
	string c;
	c.append(1, 12);
	for (i = 0; i + 65535 < u.size(); i += 65536) {
		c.append(1, 248);
		c.append(1, 255);
		c.append(1, 15);
		c.append(&u[i], 65536);
	}
	if (i < u.size()) {
		int r = u.size() - i - 1;
		c.append(1, (r & 31) << 3);
		c.append(1, r >> 5);
		c.append(1, 8 + (r >> 13));
		c.append(&u[i], r + 1);
	}
	c.append(1, 3);
	return c;
}

int main(int argc, char **argv) {
	if (argc < 2)
		return 0;
	cout << BrotliCompressTrivial(string(argv[1]));
	return 0;
}
