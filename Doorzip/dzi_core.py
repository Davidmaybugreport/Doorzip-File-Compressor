import os
import zipfile

def target_byte_compress(input_path, output_file, target_bytes):
    """
    Compresses a file, folder, or existing archive into a .dzi file,
    padding with null-bytes until the final file hits exact target_bytes.
    """
    temp_zip = "temp_compressed.zip"
    
    # 1. Build initial compressed zip file
    if os.path.isdir(input_path):
        with zipfile.ZipFile(temp_zip, 'w', zipfile.ZIP_DEFLATED, compresslevel=9) as zf:
            folder_base = os.path.dirname(input_path)
            for root, dirs, files in os.walk(input_path):
                for file in files:
                    full_path = os.path.join(root, file)
                    relpath = os.path.relpath(full_path, folder_base)
                    zf.write(full_path, relpath)
                    
    elif input_path.lower().endswith('.zip'):
        return transform_zip_to_dzi(input_path, output_file, target_bytes)
    else:
        with zipfile.ZipFile(temp_zip, 'w', zipfile.ZIP_DEFLATED, compresslevel=9) as zf:
            zf.write(input_path, os.path.basename(input_path))
    
    current_size = os.path.getsize(temp_zip)
    
    # 2. Safety Check: Target size must be >= compressed payload size
    if current_size > target_bytes:
        print(f"Error: Minimum payload ({current_size} bytes) exceeds target size ({target_bytes} bytes).")
        if os.path.exists(temp_zip):
            os.remove(temp_zip)
        return False

    # 3. Read payload and calculate exact padding
    with open(temp_zip, 'rb') as f_in:
        data = f_in.read()
        
    padding_needed = target_bytes - current_size
    
    # 4. Write payload + padding to output .dzi
    with open(output_file, 'wb') as f_out:
        f_out.write(data)
        f_out.write(b'\x00' * padding_needed)
        
    if os.path.exists(temp_zip):
        os.remove(temp_zip)
        
    print(f"Success! Created {output_file} at exact target size: {os.path.getsize(output_file)} bytes.")
    return True

def transform_zip_to_dzi(zip_file_path, output_dzi_path, target_bytes):
    """
    Takes an existing .zip file and pads it to match exact target_bytes.
    """
    if not os.path.exists(zip_file_path) or not zip_file_path.lower().endswith('.zip'):
        return False

    current_size = os.path.getsize(zip_file_path)
    if current_size > target_bytes:
        print(f"Error: Existing zip size ({current_size} bytes) exceeds target size ({target_bytes} bytes).")
        return False

    with open(zip_file_path, 'rb') as f_in:
        zip_data = f_in.read()

    padding_needed = target_bytes - current_size

    with open(output_dzi_path, 'wb') as f_out:
        f_out.write(zip_data)
        f_out.write(b'\x00' * padding_needed)

    print(f"Success! Converted zip to {output_dzi_path} at exact size: {os.path.getsize(output_dzi_path)} bytes.")
    return True

def extract_dzi(dzi_path, extract_to):
    """
    Finds the standard ZIP EOCD marker, strips trailing null-bytes,
    and extracts all contents cleanly.
    """
    if not os.path.exists(dzi_path):
        return False
        
    with open(dzi_path, 'rb') as f:
        data = f.read()
        
    eocd_sig = b'\x50\x4b\x05\x06'
    eocd_idx = data.rfind(eocd_sig)
    
    if eocd_idx == -1:
        return False
        
    if len(data) < eocd_idx + 22:
        return False
        
    comment_len_bytes = data[eocd_idx+20:eocd_idx+22]
    comment_len = int.from_bytes(comment_len_bytes, 'little')
    true_zip_size = eocd_idx + 22 + comment_len
    
    clean_zip_path = dzi_path + ".tmp.zip"
    with open(clean_zip_path, 'wb') as f:
        f.write(data[:true_zip_size])
        
    try:
        with zipfile.ZipFile(clean_zip_path, 'r') as zf:
            zf.extractall(extract_to)
        os.remove(clean_zip_path)
        return True
    except Exception:
        if os.path.exists(clean_zip_path):
            os.remove(clean_zip_path)
        return False