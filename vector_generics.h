#define vSet(v, ...)                            \
    _Generic((v),                               \
        Vector2 *:  v2Set,                      \
        Vector3 *:  v3Set,                      \
        Vector4 *:  v4Set                       \
    )(v, __VA_ARGS__)

#define vMag(v)                                 \
    _Generic((v),                               \
        Vector2:    v2Mag,                      \
        Vector3:    v3Mag,                      \
        Vector4:    v4Mag                       \
    )(v)

#define vSum(v1, v2)                            \
    _Generic((v1),                              \
        Vector2:    v2Sum,                      \
        Vector3:    v3Sum,                      \
        Vector4:    v4Sum                       \
    )((v1), (v2))

#define vAdd(v1, v2)                            \
    _Generic((v1),                              \
        Vector2 *:  v2Add,                      \
        Vector3 *:  v3Add,                      \
        Vector4 *:  v4Add                       \
    )((v1), (v2))

#define vDiff(v1, v2)                           \
    _Generic((v1),                              \
        Vector2:    v2Diff,                     \
        Vector3:    v3Diff,                     \
        Vector4:    v4Diff                      \
    )((v1), (v2))

#define vSub(v1, v2)                            \
    _Generic((v1),                              \
        Vector2 *:  v2Sub,                      \
        Vector3 *:  v3Sub,                      \
        Vector4 *:  v4Sub                       \
    )((v1), (v2))

#define mTransform(m, v)                                            \
    _Generic((m),                                                   \
        Matrix2x2:                                                  \
            _Generic(v,                                             \
                Vector2:    m2x2Transform,                          \
                default:    (Vector2 (*)(Matrix2x2, Vector2)) NULL  \
            ),                                                      \
        Matrix2x3:                                                  \
            _Generic(v,                                             \
                Vector3:    m2x3Transform,                          \
                default:    (Vector2 (*)(Matrix2x3, Vector3)) NULL  \
            ),                                                      \
        Matrix2x4:                                                  \
            _Generic(v,                                             \
                Vector4:    m2x4Transform,                          \
                default:    (Vector2 (*)(Matrix2x4, Vector4)) NULL  \
            ),                                                      \
        Matrix3x2:                                                  \
            _Generic(v,                                             \
                Vector2:    m3x2Transform,                          \
                default:    (Vector3 (*)(Matrix3x2, Vector2)) NULL  \
            ),                                                      \
        Matrix3x3:                                                  \
            _Generic(v,                                             \
                Vector3:    m3x3Transform,                          \
                default:    (Vector3 (*)(Matrix3x3, Vector3)) NULL  \
            ),                                                      \
        Matrix3x4:                                                  \
            _Generic(v,                                             \
                Vector4:    m3x4Transform,                          \
                default:    (Vector3 (*)(Matrix3x4, Vector4)) NULL  \
            ),                                                      \
        Matrix4x2:                                                  \
            _Generic(v,                                             \
                Vector2:    m4x2Transform,                          \
                default:    (Vector4 (*)(Matrix4x2, Vector2)) NULL  \
            ),                                                      \
        Matrix4x3:                                                  \
            _Generic(v,                                             \
                Vector3:    m4x3Transform,                          \
                default:    (Vector4 (*)(Matrix4x3, Vector3)) NULL  \
            ),                                                      \
        Matrix4x4:                                                  \
            _Generic(v,                                             \
                Vector4:    m4x4Transform,                          \
                default:    (Vector4 (*)(Matrix4x4, Vector4)) NULL  \
            )                                                       \
    )(m, v)

#define mSet(m, ...)                            \
    _Generic((m),                               \
        Matrix2x2 *:  m2x2Set,                  \
        Matrix2x3 *:  m2x3Set,                  \
        Matrix2x4 *:  m2x4Set,                  \
        Matrix3x2 *:  m3x2Set,                  \
        Matrix3x3 *:  m3x3Set,                  \
        Matrix3x4 *:  m3x4Set,                  \
        Matrix4x2 *:  m4x2Set,                  \
        Matrix4x3 *:  m4x3Set,                  \
        Matrix4x4 *:  m4x4Set                   \
    )(m, __VA_ARGS__)

#define mPrint(fp, m, ...)                      \
    _Generic((m),                               \
        Matrix2x2:  m2x2Print,                  \
        Matrix2x3:  m2x3Print,                  \
        Matrix2x4:  m2x4Print,                  \
        Matrix3x2:  m3x2Print,                  \
        Matrix3x3:  m3x3Print,                  \
        Matrix3x4:  m3x4Print,                  \
        Matrix4x2:  m4x2Print,                  \
        Matrix4x3:  m4x3Print,                  \
        Matrix4x4:  m4x4Print                   \
    )(fp, m, __VA_ARGS__)

#define mPrint2(fp, m) mPrint(fp, m, #m)

#define vPrint(fp, v, ...)                      \
    _Generic((v),                               \
        Vector2:  v2Print,                      \
        Vector3:  v3Print,                      \
        Vector4:  v4Print                       \
    )(fp, v, __VA_ARGS__)
