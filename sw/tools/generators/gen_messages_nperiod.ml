(*
 * XML preprocessing of messages.xml for downlink protocol
 *
 * Copyright (C) 2003-2008 ENAC, Pascal Brisset, Antoine Drouin
 *
 * This file is part of paparazzi.
 *
 * paparazzi is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * paparazzi is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with paparazzi; see the file COPYING.  If not, write to
 * the Free Software Foundation, 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 *)

open Printf

type format = string

type _type =
    Basic of string
  | Array of string * string
  | FixedArray of string * string * int

let c_type = fun format ->
  match format with
      "Float" -> "float"
    | "Double" -> "double"
    | "Int32" -> "int32_t"
    | "Int16" -> "int16_t"
    | "Int8" -> "int8_t"
    | "Uint32" -> "uint32_t"
    | "Uint16" -> "uint16_t"
    | "Uint8" -> "uint8_t"
    | "Char" -> "char"
    | _ -> failwith (sprintf "gen_messages.c_type: unknown format '%s'" format)

let dl_type = fun format ->
  match format with
      "Float" -> "DL_TYPE_FLOAT"
    | "Double" -> "DL_TYPE_DOUBLE"
    | "Int32" -> "DL_TYPE_INT32"
    | "Int16" -> "DL_TYPE_INT16"
    | "Int8" -> "DL_TYPE_INT8"
    | "Uint32" -> "DL_TYPE_UINT32"
    | "Uint16" -> "DL_TYPE_UINT16"
    | "Uint8" -> "DL_TYPE_UINT8"
    | "Char" -> "DL_TYPE_CHAR"
    | _ -> failwith (sprintf "gen_messages.dl_type: unknown format '%s'" format)

type field = _type  * string * format option

type fields = field list

type message = {
  name : string;
  id : int;
  period : float option;
  fields : fields
}

module Syntax = struct
  (** Parse a type name and returns a _type value *)
  let parse_type = fun t varname ->
    try
      let type_parts = Str.full_split (Str.regexp "[][]") t in
      match type_parts with
      | [Str.Text ty] -> Basic ty
      | [Str.Text ty; Str.Delim "["; Str.Delim "]"] -> Array (ty, varname)
      | [Str.Text ty; Str.Delim "["; Str.Text len ; Str.Delim "]"] -> FixedArray (ty, varname, int_of_string len)
      | _ -> failwith "Gen_messages: not a valid field type"
      with
      | Failure fail -> failwith("Gen_messages: not a valid array length")

  let length_name = fun s -> "nb_"^s

  let assoc_types t =
    try
      List.assoc t Pprz.types
    with
        Not_found ->
          failwith (sprintf "Error: '%s' unknown type" t)

  let rec sizeof = function
  Basic t -> string_of_int (assoc_types t).Pprz.size
    | Array (t, varname) -> sprintf "1+%s*%s" (length_name varname) (sizeof (Basic t))
    | FixedArray (t, varname, len) -> sprintf "0+%d*%s" len (sizeof (Basic t))

  let rec nameof = function
  Basic t -> String.capitalize t
    | Array _ -> failwith "nameof"
    | FixedArray _ -> failwith "nameof"

  (** Translates a "message" XML element into a value of the 'message' type *)
  let struct_of_xml = fun xml ->
    let name = ExtXml.attrib xml "name"
    and id = ExtXml.int_attrib xml "id"
    and period = try Some (ExtXml.float_attrib xml "period") with _ -> None
    and fields =
      List.map (fun field ->
        let id = ExtXml.attrib field "name"
        and type_name = ExtXml.attrib field "type"
        and fmt = try Some (Xml.attrib field "format") with _ -> None in
        let _type = parse_type type_name id in
        (_type, id, fmt))
      (List.filter (fun t -> compare (Xml.tag t) "field" = 0) (Xml.children xml)) in
    { id=id; name = name; period = period; fields = fields }

  let check_single_ids = fun msgs ->
    let tab = Array.make 256 false
    and  last_id = ref 0 in
    List.iter (fun msg ->
      if tab.(msg.id) then
        failwith (sprintf "Duplicated message id: %d" msg.id);
      if msg.id < !last_id then
        fprintf stderr "Warning: unsorted id: %d\n%!" msg.id;
      last_id := msg.id;
      tab.(msg.id) <- true)
      msgs

  (** Translates one class of a XML message file into a list of messages *)
  let read = fun filename class_ ->
    let xml = Xml.parse_file filename in
    try
      let xml_class = ExtXml.child ~select:(fun x -> Xml.attrib x "name" = class_) xml "msg_class" in
      let msgs = List.map struct_of_xml (Xml.children xml_class) in
      check_single_ids msgs;
      msgs
    with
        Not_found -> failwith (sprintf "No msg_class '%s' found" class_)
end (* module Suntax *)


(** Pretty printer of C macros for sending and parsing messages *)
module Gen_onboard = struct
  let print_field = fun h (t, name, (_f: format option)) ->
    match t with
        Basic _ ->
          fprintf h "\t  trans->put_bytes(trans->impl, dev, %s, DL_FORMAT_SCALAR, %s, (void *) _%s);\n" (dl_type (Syntax.nameof t)) (Syntax.sizeof t) name
      | Array (t, varname) ->
          let _s = Syntax.sizeof (Basic t) in
          fprintf h "\t  trans->put_bytes(trans->impl, dev, DL_TYPE_ARRAY_LENGTH, DL_FORMAT_SCALAR, 1, (void *) &%s);\n" (Syntax.length_name varname);
          fprintf h "\t  trans->put_bytes(trans->impl, dev, %s, DL_FORMAT_ARRAY, %s * %s, (void *) _%s);\n" (dl_type (Syntax.nameof (Basic t))) (Syntax.sizeof (Basic t)) (Syntax.length_name varname) name
      | FixedArray (t, varname, len) ->
          let _s = Syntax.sizeof (Basic t) in
          fprintf h "\t  trans->put_bytes(trans->impl, dev, %s, DL_FORMAT_ARRAY, %s * %d, (void *) _%s);\n" (dl_type (Syntax.nameof (Basic t))) (Syntax.sizeof (Basic t)) len name

  let print_macro_param h = function
      (Array _, s, _) -> fprintf h "%s, %s" (Syntax.length_name s) s
    | (FixedArray _, s, _) -> fprintf h "%s" s
    | (_, s, _) -> fprintf h "%s" s

  let print_macro_parameters h = function
      [] -> ()
    | f::fields ->
      fprintf h ", ";
      print_macro_param h f;
      List.iter (fun f -> fprintf h ", "; print_macro_param h f) fields

  let print_unused_param = fun unused ->
    if unused then " __attribute__((unused))" else ""

  let print_fun_param ?(unused=false) h = function
      (Array (t, _), s, _) -> fprintf h "uint8_t %s%s, %s *_%s%s" (Syntax.length_name s) (print_unused_param unused) (c_type (Syntax.nameof (Basic t))) s (print_unused_param unused)
    | (FixedArray (t, _, _), s, _) -> fprintf h "%s *_%s%s" (c_type (Syntax.nameof (Basic t))) s (print_unused_param unused)
    | (t, s, _) -> fprintf h "%s *_%s%s" (c_type (Syntax.nameof t)) s (print_unused_param unused)

  let print_function_parameters ?(unused=false) h = function
      [] -> ()
    | f::fields ->
      fprintf h ", ";
      print_fun_param ~unused h f;
      List.iter (fun f -> fprintf h ", "; print_fun_param ~unused h f) fields

  let rec size_fields = fun fields size ->
    match fields with
        [] -> size
      | (t, _, _)::fields -> size_fields fields (size ^"+"^Syntax.sizeof t)

  let size_of_message = fun m -> size_fields m.fields "0"

  let estimated_size_of_message = fun m ->
    try
      List.fold_right
        (fun (t, _, _)  r ->  int_of_string (Syntax.sizeof t)+r)
        m.fields
        0
    with
        Failure "int_of_string" -> 0

  (** Prints the messages ids *)
  let print_enum = fun h class_ messages ->
    List.iter (fun m ->
      if m.id < 0 || m.id > 255 then begin
        fprintf stderr "Error: message %s has id %d but should be between 0 and 255\n" m.name m.id; exit 1;
      end
      else fprintf h "#ifndef MESG_%s\n#define MESG_%s 0\n#endif\n" m.name m.name
    ) messages;
    fprintf h "#define MESG_MSG_%s_NB %d\n" class_ (List.length messages)

  (** Prints the macros required to send a message *)
  let print_downlink_macros = fun h class_ messages ->
    print_enum h class_ messages

end (* module Gen_onboard *)


(********************* Main **************************************************)
let () =
  if Array.length Sys.argv <> 3 then begin
    failwith (sprintf "Usage: %s <.xml file> <class_name>" Sys.argv.(0))
  end;

  let filename = Sys.argv.(1)
  and class_name = Sys.argv.(2) in

  try
    let messages = Syntax.read filename class_name in

    let h = stdout in

    Printf.fprintf h "/* Automatically generated by gen_messages_nperiod from %s */\n" filename;
    Printf.fprintf h "/* Please DO NOT EDIT */\n";

    Printf.fprintf h "/* Macros to send and receive messages of class %s */\n" class_name;
    Printf.fprintf h "#ifndef _VAR_MESSAGES_NPERIOD_%s_H_\n" class_name;
    Printf.fprintf h "#define _VAR_MESSAGES_NPERIOD_%s_H_\n" class_name;

    (** Macros for airborne downlink (sending) *)
    Gen_onboard.print_downlink_macros h class_name messages;

    Printf.fprintf h "#endif // _VAR_MESSAGES_NPERIOD_%s_H_\n" class_name

  with
      Xml.Error (msg, pos) -> failwith (sprintf "%s:%d : %s\n" filename (Xml.line pos) (Xml.error_msg msg))
