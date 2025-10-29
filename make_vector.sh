#!/bin/sh

# make_vectors.sh: XXX
#
# Copyright: (c) 2025 Jacco van Schaik (jacco@jaccovanschaik.net)
# Created:   2025-08-08
#
# This software is distributed under the terms of the MIT license. See
# http://www.opensource.org/licenses/mit-license.php for details.
#
# vim: softtabstop=4 shiftwidth=4 expandtab textwidth=80

list() {
    upto=$1
    sep=$2
    repl=$3

    max=$(expr $upto - 1)

    echo $(seq -s "$sep" 0 "$max" | sed "s/\([0-9]\+\)/$repl/g")
}

vector_typedef() {
    size=$1

    echo "typedef struct { double c[$size]; } Vector${size};"
}

matrix_typedef() {
    rows=$1
    cols=$2

    echo "typedef struct { double c[$rows][$cols]; } Matrix${rows}x${cols};"
}

vector_make() {
    size=$1

    args=$(list $size ", " "double c\1")

    cat << EOF

/*
 * Make and return a vector of size $size.
 */
Vector${size} v${size}Make($args)$2
EOF

    if [ "$2" = ';' ]; then return; fi

    args=$(list $size ", " "c\1")

    cat << EOF
{
    return (Vector${size}) { .c = { $args } };
}
EOF
}

vector_set() {
    size=$1

    args=$(list $size ", " "double c\1")

    cat << EOF

/*
 * Set the coefficients of vector <v> as given.
 */
void v${size}Set(Vector${size} *v, $args)$2
EOF

    if [ "$2" = ';' ]; then return; fi

    echo '{'

    for i in $(seq 0 $(expr $size - 1)); do
        echo "    v->c[$i] = c$i;"
    done

    echo '}'
}

vector_sum() {
    size=$1
    max=$(expr $size - 1)

    cat << EOF

/*
 * Return the sum of <v1> and <v2>.
 */
Vector$size v${size}Sum(Vector${size} v1, Vector${size} v2)$2
EOF

    if [ "$2" = ';' ]; then return; fi

    echo '{'

    initial="    return (Vector$size) { .c = { "
    indent=$(printf "%*s" $(expr length "$initial") "")

    for i in $(seq 0 $max); do
        if [ $i -eq 0 ]; then
            echo -n "$initial"
        else
            echo -n "$indent"
        fi

        echo -n "v1.c[$i] + v2.c[$i]"

        if [ $i -eq $max ]; then
            echo ' } };'
        else
            echo ','
        fi
    done

    echo '}'
}

vector_add() {
    size=$1

    cat << EOF

/*
 * Increase the vector at <v1> by <v2>.
 */
void v${size}Add(Vector${size} *v1, Vector${size} v2)$2
EOF

    if [ "$2" = ';' ]; then return; fi

    cat << EOF
{
    *v1 = v${size}Sum(*v1, v2);
}
EOF
}

vector_diff() {
    size=$1
    max=$(expr $size - 1)

    cat << EOF

/*
 * Return the difference of <v1> and <v2>, i.e. <v1> - <v2>.
 */
Vector$size v${size}Diff(Vector${size} v1, Vector${size} v2)$2
EOF

    if [ "$2" = ';' ]; then return; fi

    echo '{'

    initial="    return (Vector$size) { .c = { "
    indent=$(printf "%*s" $(expr length "$initial") "")

    for i in $(seq 0 $max); do
        if [ $i -eq 0 ]; then
            echo -n "$initial"
        else
            echo -n "$indent"
        fi

        echo -n "v1.c[$i] - v2.c[$i]"

        if [ $i -eq $max ]; then
            echo ' } };'
        else
            echo ','
        fi
    done

    echo '}'
}

vector_sub() {
    size=$1

    cat << EOF

/*
 * Reduce the vector at <v1> by <v2>.
 */
void v${size}Sub(Vector${size} *v1, Vector${size} v2)$2
EOF

    if [ "$2" = ';' ]; then return; fi

    cat << EOF
{
    *v1 = v${size}Diff(*v1, v2);
}
EOF
}

vector_mag() {
    size=$1

    cat << EOF

/*
 * Return the magnitude, or length, of vector <v>.
 */
double v${size}Mag(Vector${size} v)$2
EOF

    if [ "$2" = ';' ]; then return; fi

    exp=$(list $size " + " 'SQR(v.c[\1])')

    cat << EOF
{
    return sqrt($exp);
}
EOF
    true
}

vector_times() {
    size=$1
    max=$(expr $size - 1)

    cat << EOF

/*
 * Return vector <v>, multiplied by <factor>.
 */
Vector$size v${size}Times(Vector${size} v, double factor)$2
EOF

    if [ "$2" = ';' ]; then return; fi

    echo '{'

    initial="    return (Vector$size) { .c = { "
    indent=$(printf "%*s" $(expr length "$initial") "")

    for i in $(seq 0 $max); do
        if [ $i -eq 0 ]; then
            echo -n "$initial"
        else
            echo -n "$indent"
        fi

        echo -n "v.c[$i] * factor"

        if [ $i -eq $max ]; then
            echo ' } };'
        else
            echo ','
        fi
    done

    echo '}'
}

vector_multiply() {
    size=$1

    cat << EOF

/*
 * Multiply vector <v> by <factor>.
 */
void v${size}Multiply(Vector${size} *v, double factor)$2
EOF

    if [ "$2" = ';' ]; then return; fi

    cat << EOF
{
    *v = v${size}Times(*v, factor);
}
EOF
}

vector_normalized() {
    size=$1
    max=$(expr $size - 1)

    cat << EOF

/*
 * Return vector <v> normalized, i.e. with its length reduced to 1.
 */
Vector$size v${size}Normalized(Vector${size} v)$2
EOF

    if [ "$2" = ';' ]; then return; fi

    echo '{'
    echo "    double mag = v${size}Mag(v);"
    echo ''
    echo "    return v${size}Times(v, 1.0 / mag);"
    echo '}'
}

vector_normalize() {
    size=$1

    cat << EOF

/*
 * Normalize vector <v>, i.e. reduce its length to 1.
 */
void v${size}Normalize(Vector${size} *v)$2
EOF

    if [ "$2" = ';' ]; then return; fi

    cat << EOF
{
    *v = v${size}Normalized(*v);
}
EOF
}

vector_dot() {
    size=$1
    max=$(expr $size - 1)

    cat << EOF

/*
 * Return the dot-product of <v1> and <v2>.
 */
double v${size}Dot(Vector${size} v1, Vector${size} v2)$2
EOF

    if [ "$2" = ';' ]; then return; fi

    echo '{'

    initial="    return "
    indent=$(printf "%*s" $(expr length "$initial") "")

    for i in $(seq 0 $max); do
        if [ $i -eq 0 ]; then
            echo -n "$initial"
        else
            echo -n "$indent"
        fi

        echo -n "v1.c[$i] * v2.c[$i]"

        if [ $i -eq $max ]; then
            echo ';'
        else
            echo ' +'
        fi
    done

    echo '}'
}

vector_cross() {
    cat << EOF

/*
 * Return the cross-product of <v1> and <v2> (only defined for 3 dimensions).
 */
Vector3 v3Cross(Vector3 v1, Vector3 v2)$1
EOF

    if [ "$1" = ';' ]; then return; fi

    cat << EOF
{
    return (Vector3) { .c = { v1.c[1] * v2.c[2] - v1.c[2] * v2.c[1],
                              v1.c[2] * v2.c[0] - v1.c[0] * v2.c[2],
                              v1.c[0] * v2.c[1] - v1.c[1] * v2.c[0] } };
}
EOF
}

vector_print() {
    size=$1

    cat << EOF

/*
 * Print vector <v> to <fp>, preceded by a caption defined by the
 * printf-compatible format specifier at <caption> and the subsequent
 * paramaters.
 */
__attribute__((format (printf, 3, 4)))
void v${size}Print(FILE *fp, Vector${size} v, const char *caption, ...)$2
EOF

    if [ "$2" = ';' ]; then return; fi

    echo '{'
    echo '    va_list ap;'
    echo ''
    echo "    va_start(ap, caption);"
    echo "    va_mprint(fp, $size, 1, &v.c[0], caption, ap);"
    echo "    va_end(ap);"
    echo '}'
}

vector_va_print() {
    size=$1

    cat << EOF

/*
 * Print vector <v> to <fp>, preceded by a caption defined by the
 * printf-compatible format specifier at <caption> and the subsequent
 * paramaters.
 */
void va_v${size}Print(FILE *fp, Vector${size} v, const char *caption, va_list ap)$2
EOF

    if [ "$2" = ';' ]; then return; fi

    echo '{'
    echo "    va_mprint(fp, $size, 1, &v.c[0], caption, ap);"
    echo '}'
}

matrix_sign() {
    cat << EOF

static double m_sign(int row, int col)
{
    return (row + col) % 2 == 0 ? 1.0 : -1.0;
}
EOF
}

matrix_make() {
    rows=$1
    cols=$2

    max_row=$(expr $rows - 1)
    max_col=$(expr $cols - 1)

    cat << EOF

/*
 * Make and return a ${rows} rows by ${cols} columns matrix with the given
 * coefficients.
 */
Matrix${rows}x${cols} m${rows}x${cols}Make(
EOF

for row in $(seq 0 $max_row); do
    echo -n "        $(list $cols ', ' "double row${row}col\1" )"

    if [ $row -eq $max_row ]; then
        echo ")$3"
    else
        echo ','
    fi
done

    if [ "$3" = ';' ]; then return; fi

    echo '{'
    echo "    return (Matrix${rows}x${cols}) { .c = {"

    for row in $(seq 0 $max_row); do
        echo -n "         { "

        for col in $(seq 0 $max_col); do
            echo -n "row${row}col${col}"

            if [ $col -lt $max_col ]; then
                echo -n ', '
            fi
        done

        if [ $row -eq $max_row ]; then
            echo ' } } };'
        else
            echo ' },'
        fi
    done

    echo '}'
}

matrix_make_rows() {
    rows=$1
    cols=$2

    max_row=$(expr $rows - 1)
    max_col=$(expr $cols - 1)

    cat << EOF

/*
 * Make and return a ${rows} rows by ${cols} columns matrix from the given
 * row vectors.
 */
Matrix${rows}x${cols} m${rows}x${cols}MakeFromRows(
EOF

    echo -n "        $(list $rows ', ' "Vector${cols} row\1" )"
    echo ")$3"

    if [ "$3" = ';' ]; then return; fi

    echo '{'
    echo "    return (Matrix${rows}x${cols}) { .c = {"
    for row in $(seq 0 $max_row); do
        echo -n "         { "

        for col in $(seq 0 $max_col); do
            echo -n "row${row}.c[${col}]"

            if [ $col -lt $max_col ]; then
                echo -n ', '
            fi
        done

        if [ $row -eq $max_row ]; then
            echo ' } } };'
        else
            echo ' },'
        fi
    done

    echo '}'
}

matrix_make_cols() {
    rows=$1
    cols=$2

    max_row=$(expr $rows - 1)
    max_col=$(expr $cols - 1)

    cat << EOF

/*
 * Make and return a ${rows} rows by ${cols} columns matrix from the given
 * column vectors.
 */
Matrix${rows}x${cols} m${rows}x${cols}MakeFromColumns(
EOF

    echo -n "        $(list $cols ', ' "Vector${cols} col\1" )"
    echo ")$3"

    if [ "$3" = ';' ]; then return; fi

    echo '{'
    echo "    return (Matrix${rows}x${cols}) { .c = {"
    for row in $(seq 0 $max_row); do
        echo -n "         { "

        for col in $(seq 0 $max_col); do
            echo -n "col${col}.c[${row}]"

            if [ $col -lt $max_col ]; then
                echo -n ', '
            fi
        done

        if [ $row -eq $max_row ]; then
            echo ' } } };'
        else
            echo ' },'
        fi
    done

    echo '}'
}

matrix_set() {
    rows=$1
    cols=$2

    max_row=$(expr $rows - 1)
    max_col=$(expr $cols - 1)

    cat << EOF

/*
 * Fill the ${rows} rows by ${cols} columns matrix at <m> with the given
 * coefficients.
 */
void m${rows}x${cols}Set(Matrix${rows}x${cols} *m,
EOF

    for row in $(seq 0 $max_row); do
        echo -n "        $(list $cols ', ' "double row${row}col\1" )"

        if [ $row -eq $max_row ]; then
            echo ")$3"
        else
            echo ','
        fi
    done

    if [ "$3" = ';' ]; then return; fi

    echo '{'

    for row in $(seq 0 $max_row); do
        for col in $(seq 0 $max_col); do
            echo "    m->c[$row][$col] = row${row}col${col};"
        done

        if [ $row -lt $max_row ]; then echo ''; fi
    done

    echo '}'
}

matrix_set_rows() {
    rows=$1
    cols=$2

    max_row=$(expr $rows - 1)
    max_col=$(expr $cols - 1)

    cat << EOF

/*
 * Fill the ${rows} rows by ${cols} columns matrix at <m> with the given
 * row vectors.
 */
void m${rows}x${cols}SetFromRows(Matrix${rows}x${cols} *m,
EOF

    echo -n "        $(list $rows ', ' "Vector${cols} row\1" )"
    echo ")$3"

    if [ "$3" = ';' ]; then return; fi

    echo '{'

    for row in $(seq 0 $max_row); do
        for col in $(seq 0 $max_col); do
            echo "    m->c[$row][$col] = row${row}.c[${col}];"
        done

        if [ $row -lt $max_row ]; then echo ''; fi
    done

    echo '}'
}

matrix_set_cols() {
    rows=$1
    cols=$2

    max_row=$(expr $rows - 1)
    max_col=$(expr $cols - 1)

    cat << EOF

/*
 * Fill the ${rows} rows by ${cols} columns matrix at <m> with the given
 * column vectors.
 */
void m${rows}x${cols}SetFromColumns(Matrix${rows}x${cols} *m,
EOF

    echo -n "        $(list $cols ', ' "Vector${cols} col\1" )"
    echo ")$3"

    if [ "$3" = ';' ]; then return; fi

    echo '{'

    for row in $(seq 0 $max_row); do
        for col in $(seq 0 $max_col); do
            echo "    m->c[$row][$col] = col${col}.c[${row}];"
        done

        if [ $row -lt $max_row ]; then echo ''; fi
    done

    echo '}'
}

matrix_identity() {
    size=$1

    max_dim=$(expr $size - 1)

    cat << EOF

/*
 * Make and return a ${size}x${size} identity matrix.
 */
Matrix${size}x${size} m${size}x${size}Identity(void)$2
EOF

    if [ "$2" = ';' ]; then return; fi

    echo '{'

    initial="    return (Matrix${size}x${size}) { .c = { "
    indent=$(printf "%*s" $(expr length "$initial") "")

    for row in $(seq 0 $max_dim); do
        if [ $row -eq 0 ]; then
            echo -n "$initial"
        else
            echo -n "$indent"
        fi

        echo -n "{"

        for col in $(seq 0 $max_dim); do
            if [ $row -eq $col ]; then
                echo -n " 1"
            else
                echo -n " 0"
            fi

            if [ $col -lt $max_dim ]; then
                echo -n ","
            fi
        done

        if [ $row -eq $max_dim ]; then
            echo ' } } };'
        else
            echo ' },'
        fi
    done

    echo '}'
}

matrix_row() {
    rows=$1
    cols=$2

    max_row=$(expr $rows - 1)
    max_col=$(expr $cols - 1)

    cat << EOF

/*
 * Get row <row> from the ${rows} rows by ${cols} columns matrix <m> as
 * a ${cols}-dimensional vector.
 */
Vector$cols m${rows}x${cols}Row(Matrix${rows}x${cols} m, int row)$3
EOF

    if [ "$3" = ';' ]; then return; fi

    echo '{'

    echo "    assert(row >= 0 && row < $rows);"
    echo ''

    initial="    return (Vector${cols}) { .c = { "
    indent=$(printf "%*s" $(expr length "$initial") "")

    for col in $(seq 0 $max_col); do
        if [ $col -eq 0 ]; then
            echo -n "$initial"
        else
            echo -n "$indent"
        fi

        echo -n "m.c[row][$col]"

        if [ $col -eq $max_col ]; then
            echo ' } };'
        else
            echo ','
        fi
    done

    echo '}'
}

matrix_col() {
    rows=$1
    cols=$2

    max_row=$(expr $rows - 1)
    max_col=$(expr $cols - 1)

    cat << EOF

/*
 * Get column <col> from the ${rows} rows by ${cols} columns matrix <m> as
 * a ${rows}-dimensional vector.
 */
Vector$rows m${rows}x${cols}Column(Matrix${rows}x${cols} m, int col)$3
EOF

    if [ "$3" = ';' ]; then return; fi

    echo '{'

    echo "    assert(col >= 0 && col < $cols);"
    echo ''

    initial="    return (Vector${rows}) { .c = { "
    indent=$(printf "%*s" $(expr length "$initial") "")

    for row in $(seq 0 $max_row); do
        if [ $row -eq 0 ]; then
            echo -n "$initial"
        else
            echo -n "$indent"
        fi

        echo -n "m.c[$row][col]"

        if [ $row -eq $max_row ]; then
            echo ' } };'
        else
            echo ','
        fi
    done

    echo '}'
}

matrix_product() {
    # These are the number of rows and columns of the first matrix in the
    # multiplication. The second one will have the same sizes but reversed, the
    # result will be a square matrix with sides of size $rows.

    rows=$1
    cols=$2

    cat << EOF

/*
 * Multiply the ${rows}x${cols} matrix <m1> and the ${cols}x${rows} matrix <m2>
 * and return the resulting ${rows}x${rows} matrix.
 */
Matrix${rows}x${rows} m${rows}x${cols}Product(Matrix${rows}x${cols} m1, Matrix${cols}x${rows} m2)$3
EOF

    if [ "$3" = ';' ]; then return; fi

    cat << EOF
{
    Matrix${rows}x${rows} out = { 0 };

    for (int row = 0; row < $rows; row++) {
        for (int col = 0; col < $rows; col++) {
            Vector$cols row_vector = m${rows}x${cols}Row(m1, row);
            Vector$cols col_vector = m${cols}x${rows}Column(m2, col);

            out.c[row][col] = v${cols}Dot(row_vector, col_vector);
        }
    }

    return out;
}
EOF
}

matrix_multiply() {
    rows=$1
    cols=$2

    cat << EOF

/*
 * Multiply the ${rows}x${cols} matrix at <m1> with ${cols}x${rows} matrix <m2>.
 */
void m${rows}x${cols}Multiply(Matrix${rows}x${cols} *m1, Matrix${cols}x${rows} m2)$3
EOF

    if [ "$3" = ';' ]; then return; fi

    cat << EOF
{
    *m1 = m${rows}x${cols}Product(*m1, m2);
}
EOF
}

matrix_determinant() {
    size=$1

    max_dim=$(expr $size - 1)

    cat << EOF

/*
 * Return the determinant for the ${size}x${size} matrix m.
 */
double m${size}x${size}Determinant(Matrix${size}x${size} m)$2
EOF

    if [ "$2" = ';' ]; then return; fi

    echo '{'

    if [ $size -eq 2 ]; then
        echo '    return m.c[0][0] * m.c[1][1] - m.c[0][1] * m.c[1][0];'
    else
        initial="    return "
        indent=$(printf "%*s" $(expr length "$initial") "")

        for col in $(seq 0 $max_dim); do
            if [ $col -eq 0 ]; then
                echo -n "$initial"
            else
                echo -n "$indent"
            fi

            echo -n "m${size}x${size}Cofactor(m, 0, ${col})"

            if [ $col -lt $max_dim ]; then
                echo ' +'
            else
                echo ';'
            fi
        done
    fi

    echo '}'
}

matrix_cofactor() {
    size=$1
    max_dim=$(expr $size - 1)

    major=Matrix${size}x${size}
    minor=Matrix${max_dim}x${max_dim}

    cat << EOF

/*
 * Return the cofactor for row <row>, column <col> in the ${size}x${size} matrix
 * <m>.
 */
double m${size}x${size}Cofactor($major m, int row, int col)$2
EOF

    if [ "$2" = ';' ]; then return; fi

    echo '{'
    echo "    return m_sign(row, col) * m${size}x${size}Minor(m, row, col);"
    echo '}'
}

matrix_cofactored() {
    size=$1
    max_dim=$(expr $size - 1)

    cat << EOF

/*
 * Return a ${size}x${size} matrix that contains all the cofactors for matrix
 * <m>.
 */
Matrix${size}x${size} m${size}x${size}Cofactored(Matrix${size}x${size} m)$2
EOF

    if [ "$2" = ';' ]; then return; fi

    cat << EOF
{
    Matrix${size}x${size} out;

    for (int row = 0; row < $size; row++) {
        for (int col = 0; col < $size; col++) {
            out.c[row][col] = m${size}x${size}Cofactor(m, row, col);
        }
    }

    return out;
}
EOF
}

matrix_cofactors() {
    size=$1
    max_dim=$(expr $size - 1)

    cat << EOF

/*
 * fill the ${size}x${size} matrix at <m> with its own cofactors.
 */
void m${size}x${size}Cofactors(Matrix${size}x${size} *m)$2
EOF

    if [ "$2" = ';' ]; then return; fi

    cat << EOF
{
    *m = m${size}x${size}Cofactored(*m);
}
EOF
}

matrix_minor() {
    size=$1
    max_dim=$(expr $size - 1)

    major=Matrix${size}x${size}
    minor=Matrix${max_dim}x${max_dim}

    cat << EOF

/*
 * Return the minor for row <row>, column <col> of the ${size}x${size} matrix
 * <m>.
 */
double m${size}x${size}Minor($major m, int row, int col)$2
EOF

    if [ "$2" = ';' ]; then return; fi

    echo    '{'

    if [ $size -eq 2 ]; then
        echo "    return m.c[1 - row][1 - col];"
    else
        echo -n "    return m${max_dim}x${max_dim}Determinant("
        echo    "m${size}x${size}MinorMatrix(m, row, col));"
    fi

    echo    '}'
}

matrix_minor_matrix() {
    size=$1
    max_dim=$(expr $size - 1)

    major=Matrix${size}x${size}
    minor=Matrix${max_dim}x${max_dim}

    cat << EOF

/*
 * Return the minor ${max_dim}x${max_dim} matrix obtained by cutting out row
 * <row> and column <col> from ${size}x${size} matrix <m>.
 */
$minor m${size}x${size}MinorMatrix($major m, int row, int col)$2
EOF

if [ "$2" = ';' ]; then return; fi

cat << EOF
{
    $minor minor;

    int dst_row = 0;

    for (int src_row = 0; src_row < $size; src_row++) {
        if (src_row == row) continue;

        int dst_col = 0;

        for (int src_col = 0; src_col < $size; src_col++) {
            if (src_col == col) continue;

            minor.c[dst_row][dst_col] = m.c[src_row][src_col];

            dst_col++;
        }

        dst_row++;
    }

    return minor;
}
EOF
}

matrix_invertible() {
    size=$1
    max_dim=$(expr $size - 1)

    cat << EOF

/*
 * Return true if matrix <m> is invertible, otherwise false.
 */
bool m${size}x${size}Invertible(Matrix${size}x${size} m)$2
EOF

if [ "$2" = ';' ]; then return; fi

cat << EOF
{
    return m${size}x${size}Determinant(m) != 0;
}
EOF
}

matrix_transposed() {
    size=$1
    max_dim=$(expr $size - 1)

    cat << EOF

/*
 * Return a transposed version of ${size}x${size} matrix <m>.
 */
Matrix${size}x${size} m${size}x${size}Transposed(Matrix${size}x${size} m)$2
EOF

    if [ "$2" = ';' ]; then return; fi

    echo '{'

    echo "    return (Matrix${size}x${size}) { .c = {"

    for row in $(seq 0 $max_dim); do
        echo -n "            {"

        for col in $(seq 0 $max_dim); do
            echo -n " m.c[$col][$row]"

            if [ $col -lt $max_dim ]; then
                echo -n ","
            fi
        done

        if [ $row -eq $max_dim ]; then
            echo ' }\n    } };'
        else
            echo ' },'
        fi
    done

    echo '}'
}

matrix_transpose() {
    size=$1
    max_dim=$(expr $size - 1)

    cat << EOF

/*
 * Set the ${size}x${size} matrix at <m> to a transposed version of itself.
 */
void m${size}x${size}Transpose(Matrix${size}x${size} *m)$2
EOF

    if [ "$2" = ';' ]; then return; fi

    cat << EOF
{
    *m = m${size}x${size}Transposed(*m);
}
EOF
}

matrix_adjointed() {
    size=$1
    max_dim=$(expr $size - 1)

    cat << EOF

/*
 * Return the adjointed version of ${size}x${size} matrix <m>.
 */
Matrix${size}x${size} m${size}x${size}Adjointed(Matrix${size}x${size} m)$2
EOF

    if [ "$2" = ';' ]; then return; fi

    cat << EOF
{
    return m${size}x${size}Transposed(m${size}x${size}Cofactored(m));
}
EOF
}

matrix_adjoint() {
    size=$1
    max_dim=$(expr $size - 1)

    cat << EOF

/*
 * Set the ${size}x${size} matrix at <m> to an adjointed version of itself.
 */
void m${size}x${size}Adjoint(Matrix${size}x${size} *m)$2
EOF

    if [ "$2" = ';' ]; then return; fi

    cat << EOF
{
    *m = m${size}x${size}Adjointed(*m);
}
EOF
}

matrix_inverse() {
    size=$1
    max_dim=$(expr $size - 1)

    cat << EOF

/*
 * Return the inverse of ${size}x${size} matrix <m>.
 */
Matrix${size}x${size} m${size}x${size}Inverse(Matrix${size}x${size} m)$2
EOF

if [ "$2" = ';' ]; then return; fi

cat << EOF
{
    double det = m${size}x${size}Determinant(m);

    Matrix${size}x${size} out = m${size}x${size}Adjointed(m);

    for (int row = 0; row < $size; row++) {
        for (int col = 0; col < $size; col++) {
          out.c[row][col] /= det;
        }
    }

    return out;
}
EOF
}

matrix_invert() {
    size=$1
    max_dim=$(expr $size - 1)

    cat << EOF

/*
 * Invert the ${size}x${size} matrix at <m>.
 */
void m${size}x${size}Invert(Matrix${size}x${size} *m)$2
EOF

    if [ "$2" = ';' ]; then return; fi

    cat << EOF
{
    *m = m${size}x${size}Inverse(*m);
}
EOF
}

vector_transform() {
    rows=$1
    cols=$2
    max_row=$(expr $rows - 1)
    max_col=$(expr $cols - 1)

    cat << EOF

/*
 * Transform vector <v> through matrix <m> and return the result.
 */
Vector$rows m${rows}x${cols}Transform(Matrix${rows}x${cols} m, Vector${cols} v)$3
EOF

    if [ "$3" = ';' ]; then return; fi

    echo '{'

    echo "    return (Vector$rows) { .c = {"

    for row in $(seq 0 $max_row); do
        echo -n "        "

        for col in $(seq 0 $max_col); do
            echo -n "m.c[$row][$col]*v.c[$col]"

            if [ $col -lt $max_col ]; then
                echo -n '+'
            fi
        done

        if [ $row -lt $max_row ]; then
            echo ','
        fi
    done

    echo '\n    } };'
    echo '}'
}

matrix_va_print_any() {
    DIGITS=9    # The maximum number of digits to print in a coefficient.

    cat << EOF

/*
 * Print <rows> rows and <cols> columns of doubles, starting at <data>, to <fp>.
 * All of this preceded by a caption defined by the printf-compatible format
 * specifier at <caption> and the paramaters in <ap>.
 */
static void va_mprint(FILE *fp, size_t rows, size_t cols,
        double *data, const char *caption, va_list ap)$1
EOF

    if [ "$1" = ';' ]; then return; fi

    cat << EOF
{
    char *cap = NULL;
    int lead_in = 0;

    if (caption) {
        lead_in = 1 + asprintf(&cap, caption, ap);
    }

    int max_width[cols];
    int max_digits = 0;

    memset(max_width, 0, sizeof(max_width));

    int index = 0;

    // First find the optimal format to use. We'll try to have the columns
    // aligned and snuggled up to each other, and the same number of digits
    // shown in each coefficient. We will print a maximum of ${DIGITS} digits in
    // each coefficient.

    for (size_t row = 0; row < rows; row++) {
        for (size_t col = 0; col < cols; col++) {
            char str[16];

            snprintf(str, sizeof(str), "%-.${DIGITS}g", data[index++]);

            bool found_point = false;

            int this_width = 0;
            int this_digits = 0;

            for (char *p = str; *p != '\0'; p++) {
                this_width++;
                if (found_point) {
                    this_digits++;
                }
                else if (*p == '.') {
                    found_point = true;
                }
            }

            if (this_digits > max_digits) max_digits = this_digits;
            if (this_width  > max_width[col]) max_width[col] = this_width;
        }
    }

    index = 0;

    for (size_t row = 0; row < rows; row++) {
        const char *left_paren, *right_paren;

        if (rows == 1) {
            left_paren  = "(";
            right_paren = ")";
        }
        else if (row == 0) {
            left_paren  = "⎛";
            right_paren = "⎞";
        }
        else if (row == rows - 1) {
            left_paren  = "⎝";
            right_paren = "⎠";
        }
        else {
            left_paren  = "⎜";
            right_paren = "⎟";
        }

        if (row == 0 && cap) {
            fprintf(fp, "%-*s%s ", lead_in, cap, left_paren);
        }
        else {
            fprintf(fp, "%-*s%s ", lead_in, "", left_paren);
        }

        for (size_t col = 0; col < cols; col++) {
            fprintf(fp, "%s%*.*f", col > 0 ? "  " : "",
                    max_width[col], max_digits, data[index++]);
        }

        fprintf(fp, " %s\n", right_paren);
    }
}

EOF
}

matrix_print() {
    rows=$1
    cols=$2
    max_row=$(expr $rows - 1)
    max_col=$(expr $cols - 1)

    cat << EOF

/*
 * Print matrix <m> to <fp>, preceded by a caption defined by the
 * printf-compatible format specifier at <caption> and the subsequent
 * paramaters.
 */
__attribute__((format (printf, 3, 4)))
void m${rows}x${cols}Print(FILE *fp, Matrix${rows}x${cols} m, const char *caption, ...)$3
EOF

    if [ "$3" = ';' ]; then return; fi

    echo '{'
    echo '    va_list ap;'
    echo ''
    echo "    va_start(ap, caption);"
    echo "    va_mprint(fp, $rows, $cols, &m.c[0][0], caption, ap);"
    echo "    va_end(ap);"
    echo '}'
}

matrix_va_print() {
    rows=$1
    cols=$2
    max_row=$(expr $rows - 1)
    max_col=$(expr $cols - 1)

    cat << EOF

/*
 * Print matrix <m> to <fp>, preceded by a caption defined by the
 * printf-compatible format specifier at <caption> and the subsequent
 * paramaters.
 */
void va_m${rows}x${cols}Print(FILE *fp, Matrix${rows}x${cols} m, const char *caption, va_list ap)$3
EOF

    if [ "$3" = ';' ]; then return; fi

    echo '{'
    echo "    va_mprint(fp, $rows, $cols, &m.c[0][0], caption, ap);"
    echo '}'
}

do_header() {
    echo '#ifndef VECTORS_H'
    echo '#define VECTORS_H'
    echo ''
    echo '#include <stdio.h>'
    echo '#include <stdarg.h>'
    echo '#include <string.h>'
    echo '#include <stdbool.h>'
    echo ''

    vector_typedef 2
    vector_typedef 3
    vector_typedef 4

    for rows in $(seq 2 4); do
        for cols in $(seq 2 4); do
            matrix_typedef $rows $cols
        done
    done

    vector_make 2 ';'
    vector_make 3 ';'
    vector_make 4 ';'

    vector_set 2 ';'
    vector_set 3 ';'
    vector_set 4 ';'

    vector_sum 2 ';'
    vector_sum 3 ';'
    vector_sum 4 ';'

    vector_add 2 ';'
    vector_add 3 ';'
    vector_add 4 ';'

    vector_diff 2 ';'
    vector_diff 3 ';'
    vector_diff 4 ';'

    vector_sub 2 ';'
    vector_sub 3 ';'
    vector_sub 4 ';'

    vector_mag 2 ';'
    vector_mag 3 ';'
    vector_mag 4 ';'

    vector_times 2 ';'
    vector_times 3 ';'
    vector_times 4 ';'

    vector_multiply 2 ';'
    vector_multiply 3 ';'
    vector_multiply 4 ';'

    vector_normalized 2 ';'
    vector_normalized 3 ';'
    vector_normalized 4 ';'

    vector_normalize 2 ';'
    vector_normalize 3 ';'
    vector_normalize 4 ';'

    vector_dot 2 ';'
    vector_dot 3 ';'
    vector_dot 4 ';'

    vector_cross ';'

    vector_print 2 ';'
    vector_print 3 ';'
    vector_print 4 ';'

    matrix_identity 2 ';'
    matrix_identity 3 ';'
    matrix_identity 4 ';'

    matrix_make 2 2 ';'
    matrix_make 2 3 ';'
    matrix_make 2 4 ';'
    matrix_make 3 2 ';'
    matrix_make 3 3 ';'
    matrix_make 3 4 ';'
    matrix_make 4 2 ';'
    matrix_make 4 3 ';'
    matrix_make 4 4 ';'

    matrix_make_rows 2 2 ';'
    matrix_make_rows 2 3 ';'
    matrix_make_rows 2 4 ';'
    matrix_make_rows 3 2 ';'
    matrix_make_rows 3 3 ';'
    matrix_make_rows 3 4 ';'
    matrix_make_rows 4 2 ';'
    matrix_make_rows 4 3 ';'
    matrix_make_rows 4 4 ';'

    matrix_make_cols 2 2 ';'
    matrix_make_cols 2 3 ';'
    matrix_make_cols 2 4 ';'
    matrix_make_cols 3 2 ';'
    matrix_make_cols 3 3 ';'
    matrix_make_cols 3 4 ';'
    matrix_make_cols 4 2 ';'
    matrix_make_cols 4 3 ';'
    matrix_make_cols 4 4 ';'

    matrix_set 2 2 ';'
    matrix_set 2 3 ';'
    matrix_set 2 4 ';'
    matrix_set 3 2 ';'
    matrix_set 3 3 ';'
    matrix_set 3 4 ';'
    matrix_set 4 2 ';'
    matrix_set 4 3 ';'
    matrix_set 4 4 ';'

    matrix_set_rows 2 2 ';'
    matrix_set_rows 2 3 ';'
    matrix_set_rows 2 4 ';'
    matrix_set_rows 3 2 ';'
    matrix_set_rows 3 3 ';'
    matrix_set_rows 3 4 ';'
    matrix_set_rows 4 2 ';'
    matrix_set_rows 4 3 ';'
    matrix_set_rows 4 4 ';'

    matrix_set_cols 2 2 ';'
    matrix_set_cols 2 3 ';'
    matrix_set_cols 2 4 ';'
    matrix_set_cols 3 2 ';'
    matrix_set_cols 3 3 ';'
    matrix_set_cols 3 4 ';'
    matrix_set_cols 4 2 ';'
    matrix_set_cols 4 3 ';'
    matrix_set_cols 4 4 ';'

    matrix_row 2 2 ';'
    matrix_row 2 3 ';'
    matrix_row 2 4 ';'
    matrix_row 3 2 ';'
    matrix_row 3 3 ';'
    matrix_row 3 4 ';'
    matrix_row 4 2 ';'
    matrix_row 4 3 ';'
    matrix_row 4 4 ';'

    matrix_col 2 2 ';'
    matrix_col 2 3 ';'
    matrix_col 2 4 ';'
    matrix_col 3 2 ';'
    matrix_col 3 3 ';'
    matrix_col 3 4 ';'
    matrix_col 4 2 ';'
    matrix_col 4 3 ';'
    matrix_col 4 4 ';'

    matrix_product 2 2 ';'
    matrix_product 2 3 ';'
    matrix_product 2 4 ';'
    matrix_product 3 2 ';'
    matrix_product 3 3 ';'
    matrix_product 3 4 ';'
    matrix_product 4 2 ';'
    matrix_product 4 3 ';'
    matrix_product 4 4 ';'

    matrix_multiply 2 2 ';'
    matrix_multiply 3 3 ';'
    matrix_multiply 4 4 ';'

    matrix_minor_matrix 3 ';'
    matrix_minor_matrix 4 ';'

    matrix_minor 2 ';'
    matrix_minor 3 ';'
    matrix_minor 4 ';'

    matrix_cofactor 2 ';'
    matrix_cofactor 3 ';'
    matrix_cofactor 4 ';'

    matrix_cofactored 2 ';'
    matrix_cofactored 3 ';'
    matrix_cofactored 4 ';'

    matrix_cofactors 2 ';'
    matrix_cofactors 3 ';'
    matrix_cofactors 4 ';'

    matrix_determinant 2 ';'
    matrix_determinant 3 ';'
    matrix_determinant 4 ';'

    matrix_invertible 2 ';'
    matrix_invertible 3 ';'
    matrix_invertible 4 ';'

    matrix_transposed 2 ';'
    matrix_transposed 3 ';'
    matrix_transposed 4 ';'

    matrix_transpose 2 ';'
    matrix_transpose 3 ';'
    matrix_transpose 4 ';'

    matrix_adjointed 2 ';'
    matrix_adjointed 3 ';'
    matrix_adjointed 4 ';'

    matrix_adjoint 2 ';'
    matrix_adjoint 3 ';'
    matrix_adjoint 4 ';'

    matrix_inverse 2 ';'
    matrix_inverse 3 ';'
    matrix_inverse 4 ';'

    matrix_invert 2 ';'
    matrix_invert 3 ';'
    matrix_invert 4 ';'

    vector_transform 2 2 ';'
    vector_transform 2 3 ';'
    vector_transform 2 4 ';'
    vector_transform 3 2 ';'
    vector_transform 3 3 ';'
    vector_transform 3 4 ';'
    vector_transform 4 2 ';'
    vector_transform 4 3 ';'
    vector_transform 4 4 ';'

    matrix_va_print 2 2 ';'
    matrix_va_print 2 3 ';'
    matrix_va_print 2 4 ';'
    matrix_va_print 3 2 ';'
    matrix_va_print 3 3 ';'
    matrix_va_print 3 4 ';'
    matrix_va_print 4 2 ';'
    matrix_va_print 4 3 ';'
    matrix_va_print 4 4 ';'

    matrix_print 2 2 ';'
    matrix_print 2 3 ';'
    matrix_print 2 4 ';'
    matrix_print 3 2 ';'
    matrix_print 3 3 ';'
    matrix_print 3 4 ';'
    matrix_print 4 2 ';'
    matrix_print 4 3 ';'
    matrix_print 4 4 ';'

    echo ''
    cat vector_generics.h

    echo ''
    echo '#endif'
}

do_source() {
    echo '#include "vector.h"'
    echo ''
    echo '#include <math.h>'
    echo '#include <assert.h>'
    echo '#include <stdio.h>'
    echo '#include <stdarg.h>'
    echo '#include <stdbool.h>'
    echo ''
    echo '#define SQR(x) ((x) * (x))'

    matrix_sign

    matrix_va_print_any

    vector_make 2
    vector_make 3
    vector_make 4

    vector_set 2
    vector_set 3
    vector_set 4

    vector_sum 2
    vector_sum 3
    vector_sum 4

    vector_add 2
    vector_add 3
    vector_add 4

    vector_diff 2
    vector_diff 3
    vector_diff 4

    vector_sub 2
    vector_sub 3
    vector_sub 4

    vector_mag 2
    vector_mag 3
    vector_mag 4

    vector_times 2
    vector_times 3
    vector_times 4

    vector_multiply 2
    vector_multiply 3
    vector_multiply 4

    vector_normalized 2
    vector_normalized 3
    vector_normalized 4

    vector_normalize 2
    vector_normalize 3
    vector_normalize 4

    vector_dot 2
    vector_dot 3
    vector_dot 4

    vector_cross

    vector_print 2
    vector_print 3
    vector_print 4

    matrix_identity 2
    matrix_identity 3
    matrix_identity 4

    matrix_make 2 2
    matrix_make 2 3
    matrix_make 2 4
    matrix_make 3 2
    matrix_make 3 3
    matrix_make 3 4
    matrix_make 4 2
    matrix_make 4 3
    matrix_make 4 4

    matrix_make_rows 2 2
    matrix_make_rows 2 3
    matrix_make_rows 2 4
    matrix_make_rows 3 2
    matrix_make_rows 3 3
    matrix_make_rows 3 4
    matrix_make_rows 4 2
    matrix_make_rows 4 3
    matrix_make_rows 4 4

    matrix_make_cols 2 2
    matrix_make_cols 2 3
    matrix_make_cols 2 4
    matrix_make_cols 3 2
    matrix_make_cols 3 3
    matrix_make_cols 3 4
    matrix_make_cols 4 2
    matrix_make_cols 4 3
    matrix_make_cols 4 4

    matrix_set 2 2
    matrix_set 2 3
    matrix_set 2 4
    matrix_set 3 2
    matrix_set 3 3
    matrix_set 3 4
    matrix_set 4 2
    matrix_set 4 3
    matrix_set 4 4

    matrix_set_rows 2 2
    matrix_set_rows 2 3
    matrix_set_rows 2 4
    matrix_set_rows 3 2
    matrix_set_rows 3 3
    matrix_set_rows 3 4
    matrix_set_rows 4 2
    matrix_set_rows 4 3
    matrix_set_rows 4 4

    matrix_set_cols 2 2
    matrix_set_cols 2 3
    matrix_set_cols 2 4
    matrix_set_cols 3 2
    matrix_set_cols 3 3
    matrix_set_cols 3 4
    matrix_set_cols 4 2
    matrix_set_cols 4 3
    matrix_set_cols 4 4

    matrix_row 2 2
    matrix_row 2 3
    matrix_row 2 4
    matrix_row 3 2
    matrix_row 3 3
    matrix_row 3 4
    matrix_row 4 2
    matrix_row 4 3
    matrix_row 4 4

    matrix_col 2 2
    matrix_col 2 3
    matrix_col 2 4
    matrix_col 3 2
    matrix_col 3 3
    matrix_col 3 4
    matrix_col 4 2
    matrix_col 4 3
    matrix_col 4 4

    matrix_product 2 2
    matrix_product 2 3
    matrix_product 2 4
    matrix_product 3 2
    matrix_product 3 3
    matrix_product 3 4
    matrix_product 4 2
    matrix_product 4 3
    matrix_product 4 4

    matrix_multiply 2 2
    matrix_multiply 3 3
    matrix_multiply 4 4

    matrix_minor_matrix 3
    matrix_minor_matrix 4

    matrix_minor 2
    matrix_minor 3
    matrix_minor 4

    matrix_cofactor 2
    matrix_cofactor 3
    matrix_cofactor 4

    matrix_cofactored 2
    matrix_cofactored 3
    matrix_cofactored 4

    matrix_cofactors 2
    matrix_cofactors 3
    matrix_cofactors 4

    matrix_determinant 2
    matrix_determinant 3
    matrix_determinant 4

    matrix_invertible 2
    matrix_invertible 3
    matrix_invertible 4

    matrix_transposed 2
    matrix_transposed 3
    matrix_transposed 4

    matrix_transpose 2
    matrix_transpose 3
    matrix_transpose 4

    matrix_adjointed 2
    matrix_adjointed 3
    matrix_adjointed 4

    matrix_adjoint 2
    matrix_adjoint 3
    matrix_adjoint 4

    matrix_inverse 2
    matrix_inverse 3
    matrix_inverse 4

    matrix_invert 2
    matrix_invert 3
    matrix_invert 4

    vector_transform 2 2
    vector_transform 2 3
    vector_transform 2 4
    vector_transform 3 2
    vector_transform 3 3
    vector_transform 3 4
    vector_transform 4 2
    vector_transform 4 3
    vector_transform 4 4

    matrix_va_print 2 2
    matrix_va_print 2 3
    matrix_va_print 2 4
    matrix_va_print 3 2
    matrix_va_print 3 3
    matrix_va_print 3 4
    matrix_va_print 4 2
    matrix_va_print 4 3
    matrix_va_print 4 4

    matrix_print 2 2
    matrix_print 2 3
    matrix_print 2 4
    matrix_print 3 2
    matrix_print 3 3
    matrix_print 3 4
    matrix_print 4 2
    matrix_print 4 3
    matrix_print 4 4
}

do_usage() {
    true
}

tool=$(basename $0)
date=$(date)

cat << EOF
/*
 * GENERATED CODE. DO NOT EDIT.
 *
 * Generated by $tool at $date.
 */

EOF

if [ "$1" = '-h' ]; then
    do_header
elif [ "$1" = '-c' ]; then
    do_source
else
    do_usage
fi
