public class RefTest {
    static class A { int x = 42; }
    public static void main(String[] args) {
        A a = new A();
        System.out.println(a.x); // 42
    }
} 