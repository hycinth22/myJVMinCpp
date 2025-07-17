public class TypeCastTest {
    public static void main(String[] args) {
        int i = Integer.MAX_VALUE;
        float f = (float)i;
        System.out.println(f); // 2.14748365E9

        long l = Long.MAX_VALUE;
        double d = (double)l;
        System.out.println(d); // 9.223372036854776E18

        double d2 = 123.456;
        int i2 = (int)d2;
        System.out.println(i2); // 123

        System.out.println((int)Double.NaN); // 0
        System.out.println((int)Double.POSITIVE_INFINITY); // 2147483647
        System.out.println((int)Double.NEGATIVE_INFINITY); // -2147483648
    }
} 