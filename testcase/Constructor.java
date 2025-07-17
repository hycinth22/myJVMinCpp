public class Constructor {
    public Constructor(){
        System.out.println(42);
    }
    public static void main(String[] args) {
        System.out.println(41);
        Constructor p = new Constructor();
        System.out.println(43);
    }
} 