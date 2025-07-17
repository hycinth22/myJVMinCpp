public class StaticMethodAdd {
    public static void main(String[] args) {
        int result = add(3, 5);
        System.out.println(result);
    }
    public static int add(int a, int b) {
        return a + b;
    }
} 