public class ObjectFieldAdd {
    int x;
    int y;
    public static void main(String[] args) {
        ObjectFieldAdd o = new ObjectFieldAdd();
        o.x = 42;
        o.y = 7;
        int sum = o.x + o.y;
        System.out.println(sum);
    }
} 