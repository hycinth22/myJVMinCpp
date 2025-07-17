public class LRemTest {
    public static void main(String[] args) {
        System.out.println(10L % 3L); // 1
        System.out.println(-10L % 3L); // -1
        System.out.println(10L % -3L); // 1
        System.out.println(0L % 3L); // 0
        // System.out.println(10L % 0L); // ArithmeticException
    }
} 