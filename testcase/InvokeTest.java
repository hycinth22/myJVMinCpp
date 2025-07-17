public class InvokeTest {
    public static void main(String[] args) {
        System.out.println(add(1, 2)); // 3
        System.out.println(sub(5, 3)); // 2
    }
    static int add(int a, int b) { return a + b; }
    static int sub(int a, int b) { return a - b; }
} 