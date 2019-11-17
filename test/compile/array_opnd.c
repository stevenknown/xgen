void foo() {
    typedef struct _tag {
        int pad;
        int opnd[2];
    } S;
    S * ir;


    int * k1, k2; //k has type with 'int*'
    k1 = *(ir->opnd + 2);
    k2 = ir->opnd[2];

    int * k3; //k2 has type with 'int (*)[]'
    //k2 = &ir->opnd;
}
