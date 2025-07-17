public class DifferentObjectFieldAdd {
    static class Point {
        int x;
        int y;
    }
    public static void main(String[] args) {
        Point p = new Point();
        p.x = 42;
        p.y = 7;
        int sum = p.x + p.y;
        System.out.println(sum);
    }
} 