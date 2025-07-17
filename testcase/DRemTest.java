public class DRemTest {
    public static void main(String[] args) {
        System.out.println(5.3 % 2.0); // 1.3
        System.out.println(-5.3 % 2.0); // -1.3
        System.out.println(5.3 % -2.0); // 1.3
        System.out.println(Double.NaN % 2.0); // NaN
        System.out.println(5.3 % 0.0); // NaN
        System.out.println(0.0 % 2.0); // 0.0
        System.out.println(5.3 % Double.POSITIVE_INFINITY); // 5.3
        System.out.println(Double.POSITIVE_INFINITY % 2.0); // NaN
    }
} 