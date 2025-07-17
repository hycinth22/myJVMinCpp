public class IRemTest {
    public static void main(String[] args) {
        System.out.println(10 % 3); // 1
        System.out.println(-10 % 3); // -1
        System.out.println(10 % -3); // 1
        System.out.println(0 % 3); // 0
        // System.out.println(10 % 0); // ArithmeticException
    }
} 