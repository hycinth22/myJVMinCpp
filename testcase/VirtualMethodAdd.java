public class VirtualMethodAdd {
    public VirtualMethodAdd() {
        System.out.println(42);
    }
    public int add(int a, int b) {
        return a + b;
    }
    public static void main(String[] args) {
        VirtualMethodAdd p = new VirtualMethodAdd();
        int sum = p.add(3, 9);
        System.out.println(sum);
    }
} 