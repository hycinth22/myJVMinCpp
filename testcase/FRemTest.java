public class FRemTest {
    public static void main(String[] args) {
        System.out.println(5.3f % 2.0f); // 1.3
        System.out.println(-5.3f % 2.0f); // -1.3
        System.out.println(5.3f % -2.0f); // 1.3
        System.out.println(Float.NaN % 2.0f); // NaN
        System.out.println(5.3f % 0.0f); // NaN
        System.out.println(0.0f % 2.0f); // 0.0
        System.out.println(5.3f % Float.POSITIVE_INFINITY); // 5.3
        System.out.println(Float.POSITIVE_INFINITY % 2.0f); // NaN
    }
} 