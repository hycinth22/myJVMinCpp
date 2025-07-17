public class EdgeCaseTest {
    public static void main(String[] args) {
        System.out.println(Integer.MAX_VALUE + 1); // -2147483648
        System.out.println(Long.MIN_VALUE - 1); // 9223372036854775807
        System.out.println(Float.NaN == Float.NaN); // false
        System.out.println(Double.POSITIVE_INFINITY > 1e308); // true
        System.out.println(0.0 / 0.0); // NaN
        System.out.println(1.0 / 0.0); // Infinity
        System.out.println(-1.0 / 0.0); // -Infinity
    }
} 