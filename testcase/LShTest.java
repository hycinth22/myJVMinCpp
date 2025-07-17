public class LShTest {
    public static void main(String[] args) {
        System.out.println(8L << 2); // 32
        System.out.println(-8L >> 2); // -2
        System.out.println(8L >>> 2); // 2
        System.out.println(-8L >>> 2); // 4611686018427387902
    }
} 