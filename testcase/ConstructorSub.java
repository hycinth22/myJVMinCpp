public class ConstructorSub extends ConstructorBase {
    public ConstructorSub() {
        System.out.println(42);
    }

    public static void main(String[] args) {
        System.out.println(41);
        ConstructorSub p = new ConstructorSub();
        System.out.println(43);
    }
}
