import java.math.BigInteger;
import java.util.*;
import java.util.regex.*;
import java.io.*;
import java.nio.charset.StandardCharsets;

/**
 * Self-contained Shamir secret reconstruction without external JSON libs.
 * Usage:
 *   javac ShamirSecretSolver.java
 *   java ShamirSecretSolver < case1.json
 */
public class ShamirSecretSolver {

    // Exact rational number using BigInteger
    static final class Fraction {
        final BigInteger num;
        final BigInteger den; // always positive

        Fraction(BigInteger n, BigInteger d) {
            if (d.signum() == 0) throw new ArithmeticException("Zero denominator");
            if (d.signum() < 0) { n = n.negate(); d = d.negate(); }
            BigInteger g = n.gcd(d);
            if (!g.equals(BigInteger.ONE)) { n = n.divide(g); d = d.divide(g); }
            this.num = n;
            this.den = d;
        }

        static Fraction of(BigInteger n) { return new Fraction(n, BigInteger.ONE); }
        static Fraction zero() { return of(BigInteger.ZERO); }

        Fraction add(Fraction o) {
            BigInteger n = this.num.multiply(o.den).add(o.num.multiply(this.den));
            BigInteger d = this.den.multiply(o.den);
            return new Fraction(n, d);
        }

        Fraction subtract(Fraction o) {
            BigInteger n = this.num.multiply(o.den).subtract(o.num.multiply(this.den));
            BigInteger d = this.den.multiply(o.den);
            return new Fraction(n, d);
        }

        Fraction multiply(Fraction o) {
            return new Fraction(this.num.multiply(o.num), this.den.multiply(o.den));
        }

        Fraction divide(Fraction o) {
            if (o.num.equals(BigInteger.ZERO)) throw new ArithmeticException("Divide by zero fraction");
            return new Fraction(this.num.multiply(o.den), this.den.multiply(o.num));
        }

        BigInteger toBigIntegerExact() {
            if (!den.equals(BigInteger.ONE)) throw new ArithmeticException("Not an integer: " + this);
            return num;
        }

        @Override
        public boolean equals(Object obj) {
            if (!(obj instanceof Fraction)) return false;
            Fraction o = (Fraction) obj;
            return this.num.equals(o.num) && this.den.equals(o.den);
        }

        @Override
        public int hashCode() {
            return 31 * num.hashCode() + den.hashCode();
        }

        @Override
        public String toString() {
            return den.equals(BigInteger.ONE) ? num.toString() : num.toString() + "/" + den.toString();
        }
    }

    // Interpolate polynomial at x=0 using Lagrange with exact fractions
    static Fraction lagrangeAtZero(List<BigInteger[]> points) {
        int k = points.size();
        Fraction secret = Fraction.zero();

        for (int i = 0; i < k; ++i) {
            BigInteger xi = points.get(i)[0];
            BigInteger yi = points.get(i)[1];
            Fraction term = Fraction.of(yi);
            for (int j = 0; j < k; ++j) {
                if (i == j) continue;
                BigInteger xj = points.get(j)[0];
                // multiply by (0 - xj) / (xi - xj)  => numerator = -xj, denominator = xi - xj
                term = term.multiply(new Fraction(xj.negate(), xi.subtract(xj)));
            }
            secret = secret.add(term);
        }
        return secret;
    }

    // All combinations of indices choose(n,k)
    static List<int[]> combinations(int n, int k) {
        List<int[]> res = new ArrayList<>();
        if (k > n) return res;
        int[] comb = new int[k];
        for (int i = 0; i < k; ++i) comb[i] = i;
        while (true) {
            res.add(Arrays.copyOf(comb, k));
            int i;
            for (i = k - 1; i >= 0; --i)
                if (comb[i] != i + n - k) break;
            if (i < 0) break;
            comb[i]++;
            for (int j = i + 1; j < k; ++j) comb[j] = comb[j - 1] + 1;
        }
        return res;
    }

    // Parsed input holder
    static final class ParsedInput {
        int n;
        int k;
        LinkedHashMap<BigInteger, BigInteger> shares = new LinkedHashMap<>(); // preserves appearance order
    }

    // Parse the simple JSON structure you provided (no general-purpose JSON library needed)
    static ParsedInput parseInput(String input) {
        ParsedInput pi = new ParsedInput();

        // extract the "keys" object content
        Pattern keysPat = Pattern.compile("\"keys\"\\s*:\\s*\\{([^}]*)\\}", Pattern.DOTALL);
        Matcher mk = keysPat.matcher(input);
        if (!mk.find()) throw new IllegalArgumentException("Missing 'keys' block in input");
        String keysContent = mk.group(1);

        // find n and k inside the keys block
        Pattern nPat = Pattern.compile("\"n\"\\s*:\\s*(\\d+)");
        Pattern kPat = Pattern.compile("\"k\"\\s*:\\s*(\\d+)");
        Matcher mn = nPat.matcher(keysContent);
        Matcher mk2 = kPat.matcher(keysContent);
        if (!mn.find() || !mk2.find()) throw new IllegalArgumentException("Missing n or k in keys");
        pi.n = Integer.parseInt(mn.group(1));
        pi.k = Integer.parseInt(mk2.group(1));

        // find share blocks like: "2": { "base": "2", "value": "111" }
        Pattern sharePat = Pattern.compile("\"(\\d+)\"\\s*:\\s*\\{[^}]*?\"base\"\\s*:\\s*\"([^\"}]+)\"\\s*,[^}]*?\"value\"\\s*:\\s*\"([^\"}]+)\"\\s*\\}", Pattern.DOTALL);
        Matcher sm = sharePat.matcher(input);
        while (sm.find()) {
            String key = sm.group(1);
            String baseS = sm.group(2);
            String valS = sm.group(3);

            int x = Integer.parseInt(key);
            int base = Integer.parseInt(baseS);
            // BigInteger supports radix up to 36; input bases must be <=36
            BigInteger y = new BigInteger(valS, base);
            pi.shares.put(BigInteger.valueOf(x), y);
        }

        if (pi.shares.size() < pi.k) throw new IllegalArgumentException("Not enough shares provided for the threshold k");
        return pi;
    }

    public static void main(String[] args) throws Exception {
        // read full stdin
        String input;
        try (InputStream in = System.in) {
            byte[] all = in.readAllBytes();
            input = new String(all, StandardCharsets.UTF_8);
        }
        input = input.trim();
        if (input.isEmpty()) {
            System.out.println("Please provide JSON input via stdin (e.g. java ShamirSecretSolver < case1.json)");
            return;
        }

        ParsedInput pi = parseInput(input);

        // build share list preserving order
        List<BigInteger[]> shareList = new ArrayList<>();
        for (Map.Entry<BigInteger, BigInteger> e : pi.shares.entrySet()) {
            shareList.add(new BigInteger[]{ e.getKey(), e.getValue() });
        }

        int nAvail = shareList.size();
        int k = pi.k;

        List<int[]> combs = combinations(nAvail, k);
        Set<Fraction> secrets = new LinkedHashSet<>();
        for (int[] c : combs) {
            List<BigInteger[]> subset = new ArrayList<>();
            for (int idx : c) subset.add(shareList.get(idx));
            Fraction s = lagrangeAtZero(subset);
            secrets.add(s);
        }

        System.out.println("Total shares available: " + nAvail + "    threshold k: " + k);
        System.out.println("Combinations tested: " + combs.size());

        if (secrets.size() == 1) {
            Fraction secret = secrets.iterator().next();
            System.out.println("All combinations produce the same secret.");
            System.out.println("Reconstructed secret f(0) = " + secret);
            try {
                BigInteger intSecret = secret.toBigIntegerExact();
                System.out.println("(Exact integer value: " + intSecret + ")");
            } catch (ArithmeticException ex) {
                System.out.println("(Secret is rational, not integer: " + secret + ")");
            }
        } else {
            System.out.println("Inconsistency: different combinations produced different secrets:");
            for (Fraction s : secrets) System.out.println("  " + s);
            System.out.println("You should check shares or bases; manual polynomial solving can help debug.");
        }
    }
}
