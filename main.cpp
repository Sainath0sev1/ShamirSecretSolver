#include <bits/stdc++.h>
#include <nlohmann/json.hpp>
#include <boost/multiprecision/cpp_int.hpp>

using namespace std;
using json = nlohmann::json;
using boost::multiprecision::cpp_int;

// Convert base-N string to decimal (cpp_int for arbitrary precision)
cpp_int decodeBaseString(const string &s, int base) {
    cpp_int result = 0;
    for (char ch : s) {
        int val;
        if (isdigit(ch)) val = ch - '0';
        else val = tolower(ch) - 'a' + 10;
        result = result * base + val;
    }
    return result;
}

// Fast modular exponentiation
long long modPow(long long base, long long exp, long long mod) {
    long long result = 1;
    base %= mod;
    while (exp > 0) {
        if (exp & 1) result = (result * base) % mod;
        base = (base * base) % mod;
        exp >>= 1;
    }
    return result;
}

// Lagrange interpolation at x=0 modulo mod
long long lagrangeInterpolation(const vector<pair<long long, cpp_int>> &points, long long mod) {
    long long secret = 0;
    int k = points.size();

    for (int i = 0; i < k; i++) {
        long long xi = points[i].first;
        long long yi = (long long)(points[i].second % mod);

        long long num = 1, den = 1;
        for (int j = 0; j < k; j++) {
            if (i == j) continue;
            num = (num * (mod - points[j].first)) % mod; // (0 - xj)
            den = (den * (xi - points[j].first + mod)) % mod;
        }

        long long inv = modPow(den, mod - 2, mod); // Fermat inverse
        long long term = ((yi * num) % mod * inv) % mod;
        secret = (secret + term) % mod;
    }

    return secret;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        cerr << "Usage: " << argv[0] << " <case1.json> <case2.json> ..." << endl;
        return 1;
    }

    long long MOD = 1000000007;

    for (int f = 1; f < argc; f++) {
        ifstream in(argv[f]);
        if (!in) {
            cerr << "Error: Could not open " << argv[f] << endl;
            continue;
        }

        json j; in >> j;
        int n = j["keys"]["n"];
        int k = j["keys"]["k"];

        vector<pair<long long, cpp_int>> shares;
        for (int i = 1; i <= k; i++) {
            int base = stoi((string)j[to_string(i)]["base"]);
            string value = j[to_string(i)]["value"];
            cpp_int y = decodeBaseString(value, base);
            shares.push_back({i, y});
        }

        long long secret = lagrangeInterpolation(shares, MOD);
        cout << argv[f] << " -> Recovered Secret = " << secret << endl;
    }

    return 0;
}
