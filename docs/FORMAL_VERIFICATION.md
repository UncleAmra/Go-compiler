# Formal Verification Roadmap (Rocq / Lean 4)

The course syllabus highlights an **Extra Points Opportunity** for formal verification using **Rocq (Coq)** or **Lean 4**. This document outlines practical approaches to secure these extra points.

---

## 1. Verification Targets

Formal verification in compilers usually focuses on one of three milestones:

### Target A: Type Safety (Soundness) — Progress & Preservation
- **Language Model:** Define a formalized subset of Mini Go expressions, types, and values in Lean 4 or Rocq.
- **Theorems to Prove:**
  1. **Progress:** If an expression $e$ is well-typed ($\vdash e : \tau$), then either $e$ is a value or there exists an $e'$ such that $e \to e'$ (evaluation never gets stuck).
  2. **Preservation:** If $\vdash e : \tau$ and $e \to e'$, then $\vdash e' : \tau$ (types are preserved across evaluation steps).

### Target B: Verified Expression Compiler to Stack Machine
- Model Mini Go arithmetic and boolean expressions as an inductive type.
- Define a small abstract stack machine with instructions (`PUSH n`, `ADD`, `SUB`, `MUL`, `CMP`, `JMP`).
- Write a formal compilation function `compile : Expr -> List Instr`.
- **Correctness Theorem:**
  $$\forall e, \text{exec}(\text{compile}(e), \text{empty\_stack}) = [\text{eval}(e)]$$

---

## 2. Starter Scaffold in Lean 4

Here is a minimal specification template in Lean 4:

```lean
inductive Ty where
  | int : Ty
  | bool : Ty
  deriving Repr, DecidableEq

inductive Op where
  | add : Op
  | sub : Op
  | mul : Op
  | eq  : Op
  deriving Repr

inductive Expr where
  | num : Int -> Expr
  | boolean : Bool -> Expr
  | binOp : Op -> Expr -> Expr -> Expr
  | ifElse : Expr -> Expr -> Expr -> Expr
  deriving Repr

inductive Value where
  | vInt : Int -> Value
  | vBool : Bool -> Value
  deriving Repr

-- Operational Semantics & Typing relations can be added here
```

---

## 3. Timeline Alignment

- **Week 12 (Compiler correctness):** Formulate the semantic specification.
- **Week 13 (HW4, W algorithm):** Formalize type inference or unification.
- **Week 16–18:** Finalize proofs and documentation.
