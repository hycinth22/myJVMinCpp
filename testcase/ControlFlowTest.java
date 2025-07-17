public class ControlFlowTest {
    public static void main(String[] args) {
        int x = 2;
        switch (x) {
            case 1: System.out.println("one"); break;
            case 2: System.out.println("two"); break;
            default: System.out.println("other");
        }
        for (int i = 0; i < 3; i++) {
            if (i == 1) continue;
            System.out.print(i);
        }
        System.out.println();
    }
} 